# セッション関連リソースに関するデザイン

* 2023-06-21 arakawa (NT)
  * 草案
* 2023-12-04 arakawa (NT)
  * 問題点を整理して、APIを一部調整
* 2024-02-07 arakawa (NT)
  * セッションのライフサイクルモデルを前提に、一部を明確化
  * [注意点](#注意点) にリクエストの完全な終了に関する内容を追記

## コンセプト

* セッションを終了した際に、当該セッションに関連するリソース群を併せて解放されるようにする
  * トランザクションやステートメントオブジェクト等は、基本的に特定のセッション内で作成し、セッション終了とともに利用不可能になる
  * セッション終了時点でこれらのリソースが残存していた場合、セッションの終了とともに解放するようにする
  * 以下のケースで特に役立つ
    * 外部からセッションを強制的に終了させられた場合
    * セッションがタイムアウトで自動的に終了する場合
* Tateyama の各サービス (e.g. jogasaki) が「現在のセッション」に関する情報を取り扱いやすいようにする
  * セッション固有のオブジェクト保有領域を提供し、セッションごとに独立してデータを保持できるようにする
  * 他のセッションからアクセス不可能にすることで、データの隔離性を高める
* Tateyama の各サービス間で、リソースの授受を行えるようにする
  * リクエストに関するリソースは基本的に個々のサービス内に蓄えている状況だが、セッションに関する領域を用意し、複数のサービスからアクセス可能にすることで、サービスをまたいだリソースの利用が可能になる
  * 当面の想定は SQL, KVS 間でのトランザクションオブジェクトの共有

## デザイン方針

* `request` オブジェクトから session store を取り出せる
  * 同一のセッションであれば、複数回のリクエストを通して同一の session store を参照可能
  * `session_context` のメンバとして管理するが、 `request` オブジェクトからは `session_store` を直接入手できるようにする (`session_context` は暴露しない)
* session store にはサービスごとにオブジェクトを登録できる
  * session store に登録するオブジェクトは、「SQL実行エンジンのセッションストア」などサービスごとに1~2個のイメージ
    * 各 prepared statement などのリソースを直接ではなく、それらを保持するコンテナを登録させるイメージ
  * サービスごとに名前空間を分離したいが、ちょっといい方法が思いつかない
    * 当面は各サービスが衝突しないように心がけるのみ
* セッション終了時には、 session store 内の各要素を明示的に破棄したのち、 session store を破棄
  * このとき、登録したオブジェクトに対して、セッションの終了を通知する
  * 通知を受けたオブジェクトが関連するリソースの開放を行うことで、当該リソースのライフサイクルをセッション内に限定できる

## C++ API

* 公開性
  * 公開ヘッダ
* 名前空間
  * `::tateyama::api::server`

### `session_element` オブジェクト

```cpp
/**
 * @brief an abstract super class of session data or their container.
 * @details clients can inherit this class and define extra properties needed for individual services.
 */
class session_element {
public:
    session_element() noexcept = default;
    session_element(session_element const&) = delete;
    session_element& operator=(session_element const&) = delete;
    session_element(session_element&&) noexcept = delete;
    session_element& operator=session_element(session_element&&) noexcept = delete;
    virtual ~session_element() = default;

    /**
     * @brief disposes underlying resources of this element.
     * @throws std::runtime_error if failed to dispose this resource
     * @attention this function may not be called in some exceptional case,
     *    and the object will be destructed without calling this.
     *    In that case, the destructor must terminate rapidly without raising any exceptions.
     */
    virtual void dispose() = 0;
};
```

* 備考
  * 各サービスはこのクラスを継承し、各サービスに関連したセッション個別の情報を保持するようなクラスを作る
  * `dispose()` 関数を呼び出した場合には、当該オブジェクト上に保持した内容を破棄する
    * この操作によって、「セッション関連リソース」を破棄することになる
    * `dispose()` を別途用意したのは、デストラクタの制約が強すぎるため

### `session_store` オブジェクト

```cpp
/**
 * @brief a storage for each service to keep session-specific data.
 * @attention this storage is NOT thread-safe, clients should use this only on the request threads.
 */
class session_store {
public:
    /// @brief the element ID type
    using id_type = std::size_t;

    /**
     * @brief registers a new element as a session data.
     * @tparam T the element type
     * @param element_id the element ID to register
     * @param element the element to register
     * @returns true if successfully registered the element
     * @returns false if another element already exists in this store
     */
    template<class T> // static_assert(std::is_base_of_v<session_element, T>)
    bool put(id_type element_id, std::shared_ptr<T> element);

    /**
     * @brief obtains the stored session data.
     * @param element_id the ID of the stored data
     * @tparam T the element type
     * @returns the stored session data if it exists
     * @returns empty std::shared_ptr if it is absent, or element type was mismatched
     */
    template<class T> // static_assert(std::is_base_of_v<session_element, T>)
    [[nodiscard]] std::shared_ptr<T> find(id_type element_id) const;

    /**
     * @brief obtains the stored session data, or creates new session data and register.
     * @details if such the session data is absent, this will creates a new element data
     *     by calling `std::make_shared<T>(std::forward<Args>(args)...)`.
     * @tparam T the element type
     * @tparam Args the element constructor parameter types
     * @param element_id the target element ID
     * @param args the constructor arguments for emplacing a new element
     * @returns the stored session data if it exists
     * @returns the emplaced session data if it does not exist
     * @returns empty if the existing element type was mismatched
     */
    template<class T, class... Args> // static_assert(std::is_base_of_v<session_element, T>)
    [[nodiscard]] std::shared_ptr<T> find_or_emplace(id_type element_id, Args&&...args);

    /**
     * @brief removes the stored session data.
     * @param element_id the ID of the stored data
     * @return true if successfully removed the stored data, or it is already absent
     * @return false if the element type was mismatched
     * @attention this never call session_element::dispose() for the removed.
     */
    template<class T> // static_assert(std::is_base_of_v<session_element, T>)
    bool remove(id_type element_id);

    /**
     * @brief disposes underlying resources of this session store.
     * @throws std::runtime_error if failed to dispose this session store
     */
    void dispose();

    // ...
};
```

* デザインメモ
  * `session_store` はセッションごとに作成し、 `session_context` のメンバとして保持しておく
  * `session_store` は `request` オブジェクトから取り出せるようにする
  * 内部的には `id_type -> std::shared_ptr<session_element>` の map 構造
    * `std::dynamic_pointer_cast<T>` する部分をカプセル化している
  * reader/writer lock を使ってコンテナ上の操作を保護する
    * 基本的にリクエストスレッドからしか利用しないため、十分にコストが小さいと判断
  * `dispose()` を呼び出すと登録された `session_element` の `session_element::dispose()` を順に呼び出す
    * 各 `session_element::dispose()` の呼び出し順序は規定しない
      * 破棄順序があるなら、順序があるものをグループ化して、グループ単位で単一の `session_element` として登録すること
    * `session_store::dispose()` 内で `session_element::dispose()` が `std::runtime_error` をスローした際、キャッチ・ログして残りの `session_element::dispose()` を続行する
      * ベストエフォートで残りのリソースの破棄を行う
    * `session_store::dispose()` はサービス側から呼び出されたくないため、何らかの方法で隠しておくことが望ましい
      * 雑にやるなら `friend` で `session_context` からのみ触れる等だが、あまり美しくはない
  * `dispose()` を行うのは、 `session_context` を破棄する直前にする
    * ライフサイクル上、 `session_context` を破棄できるようになった状態になれば、以降に `session_store` を利用するものはいなくなる

## 注意点

* セッションをまたいだリソースの融通が行えなくなる
  * prepared statement や transaction など
  * セッションをまたいでリソースを利用したければ、デタッチ・再アタッチの仕組みが必要になる
* 本件を導入するにあたり、セッションを終了する際に、全てのリクエストが **完全に終了してから** セッション関連のリソースを開放する必要がある
  * これは `tgctl session kill` によるものだけではなく、Tsubakuroからの通常の `Session.close()` も含む
  * Tsubakuro でリクエストが何らかの理由で実行中であるにもかかわらず、関連するセッションを終了した場合、上記の手当てがなければリクエストの実行中に関連するリソースが解放されてしまい、リクエストが解放済みのリソースを利用しようとすることで、データベースプロセスがクラッシュする恐れがある
