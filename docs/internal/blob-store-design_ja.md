# BLOB データの永続化に関するデザイン

## この文書について

* Tsurugi の BLOB データの永続化、および永続化した BLOB データの参照についてデザイン案を提示し、問題点の発見や作業に必要な工程・工数の見積もりを行う
* 本文書は、サーバ内において BLOB データの永続化、および永続化された BLOB データの参照に限って記述し、クライアントとサーバ間の BLOB データの授受に関しては他のデザイン文書 ([IPC 接続における BLOB データ授受に関するデザイン](./blob-ipc-design_ja.md)) に任せる
  * 具体的には、SQLサービス等のサービス内で、新たに BLOB を登録・永続化する方法と、登録済みの BLOB を参照し、BLOB の内容を取得する方法について解説する

## 基本的な考え方

* BLOB ファイルはトランザクションのコミットにより永続化し、データストア上で管理する
* インメモリインデックスやSQLエンジンは BLOB ファイルへの参照のみを記録・管理し、原則的に BLOB ファイルの内容はメモリ上に展開しない
* 個々の BLOB ファイルは、原則的にファイルシステム上の独立したファイルとして保存し、WAL には BLOB ファイルへの参照のみを記録する

## BLOB に関する制約

* BLOB データは永続化後、変更も明示的な削除もできない
  * BLOB データはあくまでレコード上のカラムの一部であり、変更は新しいバージョンの BLOB ファイルを追加するのみで、削除は何も行わない
  * 物理的に BLOB データを削除するのは、当該 BLOB を含むレコードのバージョンが誰からも参照されなくなった際に自動的に行う
* BLOB の参照と本体は 1-1 に対応する
  * 同一の BLOB データを複数の行やテーブルで保有したい場合、保有した数だけコピーが作成される
  * これを回避するには、テーブル正規化によって BLOB データを別テーブルに追い出すのがよい
  * これは、BLOB のガベージコレクションを簡略化するための措置であり、よい方法があれば制約から取り除いてもよい

## 基本的な流れ

### 流れ: BLOB データ登録

コミット前にBLOBデータをデータストアに仮登録し、コミット時に本登録するという方式をとる。

* (1) SQL エンジンは KVS 経由で BLOB データを仮登録し、 BLOB 参照を得る ([BLOB の仮登録](#境界-blob-の仮登録))
  * KVS はデータストアに BLOB データを仮登録し、BLOB 参照を得る
  * データストアは仮登録された BLOB データに対応する BLOB ファイルを作成する
  * 現在のトランザクションがアボートされた場合、仮登録された BLOB データは全て除去する
* (2) SQL エンジンは KVS にエントリを追加する際、エントリに含まれる BLOB 参照の一覧 (BLOBリスト) を併せて渡す ([BLOB を含むエントリの書き込み](#境界-blob-を含むエントリの書き込み))
  * BLOB リストとは別に、エントリ側にも BLOB 参照をエンコードして通常のカラムの値と同様に書き込む
  * コミット時に BLOB リストを利用するため、 CC はこの情報を覚えておく必要がある
* (3) SQL エンジンが KVS に対してコミットを要求する ([BLOB の永続化](#境界-blob-の永続化))
  * KVS がデータストアにエントリを書き込む際、エントリに関連する BLOB リストを併せて渡す
  * データストアは渡された BLOB リストに含まれる仮登録された BLOB データを本登録する
    * 本登録によって実際に永続化された、という扱いになる
    * すでに本登録されていた場合、エラーとなる

なお、仮登録または本登録された BLOB データのことを「BLOB ファイル」とよび、実際にファイルシステム上のファイルとして存在している前提とする。

### 流れ: BLOB データ参照

データストアに BLOB 参照を渡すと、BLOB ファイルを取り出せる。

* (1) SQL エンジンは KVS 経由でエントリを取得した際、エントリには BLOB 参照が含まれている場合がある ([BLOB 参照の取得](#境界-blob-参照の取得))
  * BLOB 参照は軽量であるため、そのままクライアントに返せる
* (2) SQL エンジンに BLOB データの取得要求があった場合、渡される BLOB 参照をもとにデータストアに BLOB ファイルを問い合わせる ([BLOB データの取得](#境界-blob-データの取得))
  * 仮登録、本登録のいずれかであれば BLOB ファイルを取り出せる

----
Note:

BLOB データの登録と参照には意図的な非対称性が含まれている。
例えば、本登録は KVS から行うが、参照はデータストアから直接行うなどが挙げられる。
これは、登録にはトランザクション処理が必要であるのに対し、参照には必要ないためである。

### 流れ: BLOB データ削除

BLOBデータの削除は、主にリストア時に行うものと、コンパクション時に行うものの2種類がある。

* (1) リストア時に BLOB データを削除する
  * 起動時に全ての WAL エントリを走査する場合、それらに含まれる全ての BLOB 参照のいずれにも対応しない BLOB ファイルをすべて削除してもよい
* (2) コンパクション時に BLOB データを削除する
  * コンパクションによって WAL 上のエントリが削除される際、当該エントリに付随する BLOB 参照が指す BLOB ファイルを削除してもよい

----
Note:

仮登録のまま滞留している BLOB ファイルが存在する場合、 (1) によってのみ削除されることになる。
そもそも仮登録のまま残留してしまうのは、クラッシュやトランザクション残存など再起動を要求するような状況であるため、このようにデザインした。

ただし、インデックスリストアが WAL 全域をスキャンしないような場合、 (1) は実現できなくなるため、別の方法も検討が必要かもしれない。

## 境界デザイン

### 境界: BLOB の仮登録

BLOB を含むエントリを書き込むには、事前にそれらの BLOB をデータストアに仮登録しておく必要がある。

BLOB データの仮登録は、BLOB データの永続化を行う前に、その BLOB データに対応する BLOB 参照を作成し、SQL 実行エンジンなどで利用可能にするためのものである。

BLOB データの仮登録にはまず「BLOB プール」というオブジェクトをデータストアから取得し、オブジェクトに対して BLOB データを仮登録することで BLOB 参照を得られる。
また、同オブジェクトを破棄することで、それまでに仮登録された BLOB データのうち、本登録が行われなかったものをすべて除去できる。

BLOB プールの作成、破棄、および BLOB データの仮登録は SQL 実行エンジン内で行うことを想定している。

`limestone::api::datastore` に以下のような BLOB 仮登録関連の API を追加する:

```cpp
class datastore {
public:
    /**
     * @brief acquires a new empty BLOB pool.
     * @details This pool is used for temporary registration of BLOBs,
     *      and all BLOBs that are not fully registered will become unavailable when the pool is destroyed.
     * @return the created BLOB pool
     * @see blob_pool::release()
     * @attention the returned BLOB pool must be released by the blob_pool::release() after the usage, or it may cause leaks of BLOB data.
     * @attention Undefined behavior if using pool after destroying this datastore.
     */
    [[nodiscard]] std::unique_ptr<blob_pool> acquire_blob_pool();
};
```

```cpp
/// @brief BLOB reference type.
using blob_id_type = std::uint64_t;

/**
 * @brief represents a pool for provisional registration of BLOB data.
 */
class blob_pool {
public:

    /**
     * @brief creates a new object.
     */
    blob_pool() = default;

    /**
     * @brief destroys this object.
     */
    virtual ~blob_pool() = default;

    blob_pool(blob_pool const&) = delete;
    blob_pool(blob_pool&&) = delete;
    blob_pool& operator=(blob_pool const&) = delete;
    blob_pool& operator=(blob_pool&&) = delete;

    /**
     * @brief Discards all BLOB data provisionally registered in this pool, except for those that have already been persistent.
     * @note After this operation, this pool will be unusable.
     * @note This operation is idempotent.
     * @attention Undefined behavior if attempting to access the data of non-persistent BLOBs in this pool after this operation.
     *     It depends on the implementation when the BLOB data is actually removed.
     */
    virtual void release() = 0;

    /**
     * @brief registers a BLOB file provisionally into this BLOB pool.
     * @param is_temporary_file true to allow remove the source file, or false to copy the source file
     * @return the corresponding BLOB reference
     * @attention This only act as provisional registration for the BLOB, and it may be lost after release() was called.
     *     To avoid it, you need to pass the BLOB references to log_channel::add_entry() to persistent them.
     * @throws std::invalid_state if this pool is already released
     */
    [[nodiscard]] virtual blob_id_type register_file(std::filesystem::path const& file, bool is_temporary_file) = 0;

    /**
     * @brief registers a BLOB data provisionally into this BLOB pool.
     * @param data the target BLOB data
     * @return the corresponding BLOB reference
     * @attention This only act as provisional registration for the BLOB, and it may be lost after release() was called.
     *     To avoid it, you need to pass the BLOB references to log_channel::add_entry() to persistent them.
     * @throws std::invalid_state if this pool is already released
     */
    [[nodiscard]] virtual blob_id_type register_data(std::string_view data) = 0;

    /**
     * @brief duplicates the registered BLOB data, and registers the copy provisionally into this BLOB pool.
     * @param reference the source BLOB reference
     * @return the corresponding BLOB reference of the duplicated one
     * @attention This only act as provisional registration for the BLOB, and it may be lost after release() was called.
     *     To avoid it, you need to pass the BLOB references to log_channel::add_entry() to persistent them.
     * @throws std::invalid_state if this pool is already released
     */
    [[nodiscard]] virtual blob_id_type duplicate_data(blob_id_type reference) = 0;
};
```

----
Note:

事前に仮登録を行うのは、仮登録によって得た BLOB 参照をエントリの中に書き込む必要があるため。

----
Note:

`duplicate_data` は、同一のデータを持つ BLOB 参照を複製するための関数である。
これは、主に BLOB の参照と本体の 1-1 対応を保つために利用し、この関数を利用することで高速に BLOB 参照を複製できる (内部的にはハードリンクを作成するなど)。

----
Note:

`blob_pool` は、実際には `datastore` に対して各操作を移譲するだけの実装になるかもしれない。
なぜなら、仮登録状態であっても `datastore` 経由で BLOB の本体を取得できる必要があるため、仮登録情報は `datastore` に集約しなければならないためである。

場合によっては、 `blob_pool` は仮登録情報を保持するだけのデータ構造としてしまって、すべての関数を `datastore` 側で提供することも検討する。

### 境界: BLOB を含むエントリの書き込み

BLOB を含むエントリを KVS に書き込む際には、エントリに含まれる BLOB 参照の一覧 (BLOB リスト) を併せて渡す必要がある。

```cpp
// namespace sharksfin

extern "C" StatusCode content_put_with_blobs(
        TransactionHandle transaction,
        StorageHandle storage,
        Slice key,
        Slice value,
        blob_id_type const* blobs_data,
        std::size_t blobs_size,
        PutOperation operation = PutOperation::CREATE_OR_UPDATE);
```

KVS はこれらの BLOB リストの内容を記録しておき、コミット時にデータストアに渡すことになる。
この時点ではまだコミット前のため、それぞれの BLOB は本登録されない。

----
Note:

書き込むエントリに BLOB 参照が含まれる場合、必ず BLOB リストにそれらの BLOB が全て含まれていなければならない。
BLOB リストは主に仮登録の BLOB を本登録に切り替え、BLOB データを永続化状態にする際の対象を特定するために利用する。
これを誤ると以下のような問題が発生する懸念がある:

* BLOB ファイルを仮登録のままにしておくと、ガベージコレクションによって除去されてしまう可能性がある
* BLOB ファイルが本登録されないと、レプリケーションやバックアップ時に複製の対象にならない場合がある
* 参照と本体の 1-1 対応が崩れ、予期せぬタイミングで BLOB ファイルが除去される可能性がある

これを回避するためには、例えば以下のような実装が考えられる。

1. 中間データにおいて仮登録の BLOB と、テーブルから取り出した BLOB の表現を変える
2. テーブルへの書き込み時に:
   * 仮登録の BLOB を発見したら、そのまま BLOB リストに追加する
   * テーブルから取り出した BLOB を発見したら、 BLOB ファイルのコピーと仮登録を行ったうえで、仮登録時の参照を BLOB リストに追加する

### 境界: BLOB の永続化

BLOB を含むエントリの書き込みを行ったトランザクションをコミットする際、データストアは BLOB リストに含まれる BLOB を本登録する。
データストアに BLOB リストを渡すには、 `limestone::api::log_channel::add_entry()` の引数にそれらを指定する (現状では `large_object_input` という名前になっているが、これを廃止して単に `blob_id_type` のリストを渡すだけでよい)。

```cpp
class log_channel {
public:
    void add_entry(
            storage_id_type storage_id,
            std::string_view key,
            std::string_view value,
            write_version_type write_version,
            std::vector<blob_id_type> const& large_objects);
};
```

ここに渡された BLOB リストは、ガベージコレクションのため WAL にも書き込んでおくとよい。
詳しくは [BLOB データ削除](#流れ-blob-データ削除) を参照。

トランザクションがコミット前にアボートされた場合、仮登録された BLOB データは全て除去する必要がある。

----
Note:

少々厄介な要件であるが、仮登録から本登録の際にファイルパスが変更されないことが望ましい。
もっとも単純な方法として、仮登録と本登録でファイルシステム上は特に区別せず、クラッシュ等によって区別がつかなくなった場合にはガベージコレクションの仕組みによって仮登録のものを除去するなどが考えられる。

### 境界: BLOB 参照の取得

BLOB 参照は KVS に登録したエントリに含まれているため、ここでは特別な操作は必要ない。

----
Note:

登録時に指定していた BLOB リストを取り出すような API は提供しない。
これは、インデックスに余計なデータを記録しないためと、インデックスのデザイン変更を不要にするため。

### 境界: BLOB データの取得

BLOB データを取得するには、関連する BLOB 参照をデータストアに渡すことで BLOB ファイルに関するオブジェクトを取得できる。

```cpp
class datastore {
public:

    /**
     * @brief returns BLOB file for the BLOB reference.
     * @param reference the target BLOB reference
     * @return the corresponding BLOB file
     * @return unavailable BLOB file if the ID is not valid
     * @attention the returned BLOB file is only available
     *    during the transaction that has provided the corresponded BLOB reference.
     */
    [[nodiscard]] blob_file get_blob_file(blob_id_type reference);
};
```

```cpp
/**
 * @brief represents a BLOB file that can provide persistent BLOB data.
 */
class blob_file {
public:

    /**
     * @brief retrieves the path to the BLOB file.
     * @returns BLOB file path
     */
    [[nodiscard]] std::filesystem::path const& path() const noexcept;

    /**
     * @brief returns whether this BLOB file is available.
     * @return true if this is available
     * @return false otherwise
     */
    [[nodiscard]] explicit operator bool() const noexcept;
};
```

----
Note:

`get_blob_file()` が「そのトランザクションの間に限り有効」としているのは、以下の理由からなる:

* 引数に渡した BLOB reference は、(1) 現在のトランザクションによって新たに作成されたか、または (2) ほかのトランザクションによって作成され、かつ現在のトランザクションから参照可能な行から取得したものである
  * (1) の場合、現在のトランザクションが abort された場合、仮登録状態の BLOB が除去された結果、当該 BLOB ファイルを利用不可能になる場合がある
  * (2) の場合、現在のトランザクションが終了した結果、[境界: BLOB データの削除](#境界-blob-データの削除) における削除の対象となり、当該 BLOB ファイルを利用不可能になる場合がある
* 上記、いずれの場合も、現在のトランザクションが有効である限りは当該 BLOB ファイルは利用可能であることが保証されている

ただし、これはデータストア単体の規約ではなく、CC との組み合わせによる invariant であるため、記述は多少曖昧になっている。

### 境界: BLOB データの削除

BLOB データを削除するには、その BLOB への参照を有するバージョンのエントリが誰からも参照されなくなっていることが必要となる。
これを判断するための情報は、 CC からデータストアに通知が行われる。

```cpp
class datastore {
public:

    /**
     * @brief change the available boundary version that the entries may be read.
     * @details The boundary version must be monotonic, that is,
     *    it must be greater than or equal to the previous boundary version.
     * @details This version comprises the oldest accessible snapshot, that is,
     *    the datastore may delete anything older than the version included in this snapshot.
     * @param version the target boundary version
     * @attention this function should be called after the ready() is called.
     * @see switch_safe_snapshot()
     * @note the specified version must be smaller than or equal to the version that was told by the switch_safe_snapshot().
     */
    void switch_available_boundary_version(write_version_type version);
};
```

上記のバージョンは、以下の両方を満たすものである。

* セーフスナップショットのバージョンと等しいか、それよりも小さい
* 現在、RTX によって読み出されているセーフスナップショットがあれば、そのいずれのバージョンと等しいか、それよりも小さい

----
Note:

注意すべき点として、この関数にの引数に渡されたバージョンいかのエントリを **すべて削除してよいという意味ではない**。
このバージョンをもとに内部的にスナップショットを構築し、そのスナップショットを境界として各エントリについてそのスナップショット上のバージョンよりも古いバージョンを消してよいことになる。

以下に例を示す:

* 前提: エントリ x にバージョン 10, 20, 30 が存在している
* 関数にバージョン 5 が指定された場合
  * スナップショットにはエントリ x が含まれないため、エントリ x に関しては何も削除しない
* 関数にバージョン 15 が指定された場合
  * スナップショットにはエントリ x のバージョン 10 が含まれる
  * エントリ x にはバージョン 10 未満のものが存在しないため、何も削除しない
* 関数にバージョン 25 が指定された場合
  * スナップショットにはエントリ x のバージョン 20 が含まれる
  * エントリ x にはバージョン 20 未満のバージョン 10 のものが存在するため、それを削除する

## 付録

### SQL 上の BLOB の扱い

* 制約
  * 比較できない
  * インデックスキーに指定できない
  * グループキーに指定できない
  * ソートキーに指定できない
* やれてもいいこと
  * `VARBINARY` / `VARCHAR` から `BLOB` / `CLOB` へのキャスト
  * `BLOB` / `CLOB` をスライスして `VARBINARY` / `VARCHAR` を取得 (`SUBSTRING` など)
  * `BLOB` / `CLOB` の長さを取得 (`LENGTH` など)
    * 頻出するなら最適化して高速にアクセスできてもよいか

### 小さな BLOB データを WAL に直接書き込む

記録する BLOB ファイルが閾値よりも小さい (~数KB程度) 場合、 BLOB typed VARBINARY としてインデックスや WAL 上に書き込んでもよい。
この場合、BLOB ファイルは作成せず、BLOB 参照の代わりに BLOB データ本体を返すことになる。
