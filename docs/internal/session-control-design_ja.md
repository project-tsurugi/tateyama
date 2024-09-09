# セッションをコントロールするCLIのデザイン

* 2022-11-01 arakawa (NT)
  * 初版
* 2023-11-30 arakawa (NT)
  * セッションスイッチに関するサブコマンドを追加
  * アーキテクチャ案を追加
* 2024-02-02 arakawa (NT)
  * CLI仕様からファイルを分離
  * セッションスイッチをセッション変数に変更
* 2024-02-06 arakawa (NT)
  * [C++ headers](#c-headers) を追加
  * セクションの順序を変更
* 2024-02-07 arakawa (NT)
  * 一部の項目を詳細化
* 2024-02-08 arakawa (NT)
  * レビューに併せて改稿

## この文書について

* [#75 セッションをコントロールするCLI](https://github.com/project-tsurugi/tsurugi-issues/issues/75) の実現方針

## 基本的なデザイン

* セッションを制御するCLIを提供する
  * 新たに開始することはできず、セッションの表示、停止のみ
  * DBAおよびテスト・デバッグ向け用途
  * 上記用途のため、Java API は不要

## アーキテクチャ

### アーキテクチャコンセプト

* 基本的に Tateyama Framework の作法にのっとり、セッションを管理する endpoint での変更を最小限にする
  * 性能や一部機能のため、多少は endpoint 側に変更が発生する

### デザイン方針

* `sessions` リソースを新規追加
  * セッションの情報を endpoint をまたいで管理するリソース
  * endpoint がセッションを開始する際に対応するセッション情報を追加し、セッションを終了する際に除去する
* `sessions` サービスを新規追加
  * 上記リクエストを受け付けて、 `sessions` リソースを操作する
  * service message version における service symbolic ID は `session`
* `session_context` オブジェクトを新規作成
  * 以下の機能を有する
    * セッション情報 ([SessionInfo](#sessioninfo) 相当) の提供
    * セッション変数の変更
    * セッション変数の取得
    * セッション終了要求
  * 以下で共有する
    * `sessions` リソース (非const)
      * `session list/show` リクエストに対し、各セッションの情報を入手する
      * `session set` リクエストに対し、セッション変数の値を書き込む
      * `session shutdown` リクエストに対し、セッション終了要求を書き込む
    * endpoint (非const)
      * `request` オブジェクト参照時に const参照を渡す
      * セッション終了要求が記録されていたら当該セッションをシャットダウンする
    * `request` オブジェクト (const)
      * 任意のサービスでセッション情報を入手する
      * 任意のサービスでセッション変数の値を確認する
* セッション変数 (`session set`) は事前登録制とする
  * 各サービスの初期化時に `sessions` リソースにキーと値の型、および初期値登録しておいて、 `session set` コマンドでは事前に確保された領域に、所定のデータ型で登録する
* セッション変数の名称は以下の文字から組み立てる
  * 数字 (0-9)
  * 英字アルファベット小文字 (a-z)
  * アンダースコア (_)
  * ドット (.)

### ライフサイクルのデザイン

* `session_context` オブジェクトが破棄される前に、当該セッション内で発生したリクエストはすべて **完全に終了していなければならない**
  * リクエストが完全に終了している、とは以下をすべて満たす必要がある
    * `session_context` オブジェクトまたはそれが所有するオブジェクトに一切触らない
    * 当該リクエストの `request` オブジェクトまたはそれが所有するオブジェクトに一切触らない
    * 当該リクエストの `response` オブジェクトまたはそれが所有するオブジェクトに一切触らない
  * 逆に言えば、上記を満たす限り、リクエスト中に発生したオブジェクト等が残留していても、リクエストは「完全に終了している」といえる
    * そのため、上記を満たせばバックグラウンドスレッド等が残留していても `session_context` を破棄してよい
    * 完全に終了している例:
      * あるリクエストによってspawnしたスレッドが、以降 `request` や `response` オブジェクト関連に一切触らない場合
      * あるリクエストがレスポンスを返した後、 `request` や `response` オブジェクトとは無関係のリソースを時間をかけて破棄する場合
  * 本アーキテクチャは、上記を前提に構成している
    * つまり、正常な状態であれば、全てのリクエストは `session_context` が生存していることを前提にしたプログラミングができるようになっている
    * このため、 `session shutdown` によって直ちに使用中のリソースが破棄されてしまうような状況を想定しない
* バックグラウンド実行中のリクエストの完全な終了を保証するために、「キャンセル要求」と「完全な終了の報告」を導入する
  * endpoint はセッションで実行中のリクエストを終了させるために「キャンセル要求」を送り、それを受け取ったサービスはその処理を完全に終了させたうえで「完全な終了の報告」を endpoint に通知する
  * 「完全な終了の報告」はサービスが `std::shared_ptr<{request, response}>` を破棄することで行う
    * リクエスト開始時にサービスへ渡された `std::shared_ptr<request>`, `std::shared_ptr<response>` の両方が endpoint 外部から参照されていない時点(参照カウント等により確認可能)で endpoint は当該リクエストが完全に終了したものとみなす
  * 基本的なデザインは Java のスレッドの `interrupt` に近い

## 登場人物

* [`sessions_core` オブジェクト](#sessions_core-オブジェクト)
  * `sessions` リソースの実体で、以下のオブジェクトを提供する
    * [`session_container` オブジェクト](#session_container-オブジェクト)
    * [`session_variable_declaration_set` オブジェクト](#session_variable_declaration_set-オブジェクト)
  * 基本的に、 `sessions` サービスと endpoint から利用するのみ
* [`session_container` オブジェクト](#session_container-オブジェクト)
  * セッションの一覧を保持するオブジェクトで、 `sessions_core` オブジェクトに一つだけ存在する
  * session ID や session labels からセッションを識別し、当該セッションに対する `session_context` (下記) を取り出す
* [`session_context` オブジェクト](#session_context-オブジェクト)
  * セッションごとに作成するコンテキストオブジェクト
  * endpoint がオーナーシップを有し、 `sessions` リソース上に borrow される
  * `session {list,show}` に必要な情報を有し、 `session {shutdown,set}` による状態の変更を受け入れる
  * `request` オブジェクトからは直接暴露させず、個々に含まれるオブジェクトのみを提供することになる
    * オーナーシップの関係で、 `request` オブジェクトは内部的に `session_context` オブジェクトを所有しておいた方がいいかもしれない
* [`session_variable_set` オブジェクト](#session_variable_set-オブジェクト)
  * セッション変数を格納するオブジェクトで、 `session_context` が直接所有する
  * `session set` 経由で値を変更でき、各サービスから現在の変数の値を取り出せる
  * **`request` オブジェクトから利用可能**
    * 現状では `request` オブジェクトからの値の変更を想定していないが、普通に変更できた方がいい気はするので非 `const` で所有すべきか
  * セッション変数を新しく定義することはできず、定義は `session_variable_declaration_set` オブジェクト経由で行う
* [`session_variable_declaration_set` オブジェクト](#session_variable_declaration_set-オブジェクト)
  * `session_variable_set` オブジェクトの内容 (セッション変数群) を定義するオブジェクトで、 `sessions_core` オブジェクトに一つだけ存在する
  * このオブジェクト自体は個々のセッション変数の状態を管理せず、その定義だけを有する
  * このオブジェクトから `session_variable_set` オブジェクトを新しく作成できる
* session shutdown リクエストフラグ
  * 対象のセッションに対して終了が要求されたかどうかを表すフラグで、 `session_context` 上で管理する
  * `session shutdown` 経由で値を変更でき、endpoint がこの値をポーリングして、終了リクエストが送られたら所定の方法で速やかにセッションを終了させる
  * フラグにはいくつか種類がある:
    * `GRACEFUL` - 新しいリクエストを閉鎖し、現状のリクエストにすべてレスポンスを返したのちにセッションを終了する
      * 新しいリクエストには、直ちに `SESSION_CLOSED` がレスポンスとして返される
      * 例外的に expiration 更新などのリクエストだけは受け入れてもよい
    * `FORCEFUL` - 新しいリクエストを閉鎖し、現状のリクエストにキャンセル要求を投げ、すべてのキャンセル要求を承認されたのちにセッションを終了する
      * 新しいリクエスト、および現在稼働中のリクエストには、直ちに `SESSION_CLOSED` がレスポンスとして返される
      * こちらは expiration 更新のリクエストを受け入れる必要はない (expiration 時には上記と同様の挙動になる)
* TBD
  * 同様に、 session expiration のための時刻情報を同オブジェクトが有してもよさそう
  * endpoint は リクエストのたびに同情報を最新時刻に更新し、 session shutdown リクエストフラグと同様にポーリングして、既定の時間を超えたら session shutdown と同様の処理を行う

## C++ headers

* 公開性
  * 基本的に公開する
    * `sessions` リソース関連は他サービスから触るため (セッション変数の定義等)
  * 例外的に、 `sessions` サービスそのものは非公開
* 名前空間
  * `::tateyama::session`
    * セッション管理に関連するオブジェクトの名前空間
    * 一部のものは `::tateyama::api::server` 配下のオブジェクト( `request` 等) を経由してサービスに提供される
    * セッションとリクエスト両方にまたがるものは `::tateyama::session` と `::tateyama::api::server` のどちらに属するか
      * 原則として前者だが、リクエスト・レスポンスに特に結びつきの強いものは後者に配置してもよい

### `sessions_core` オブジェクト

```cpp
/**
 * @brief the core class of `sessions` resource that provides information about living sessions.
 */
class sessions_core {
public:
    /**
     * @brief returns the session container.
     * @return the session container
     */
    session_container& sessions() noexcept;

    /// @copydoc sessions()
    session_container const& sessions() const noexcept;

    /**
     * @brief returns the session variable declarations.
     * @return the session variable declarations.
     */
    session_variable_declaration_set& variable_declarations() noexcept;

    /// @copydoc variable_declarations()
    session_variable_declaration_set const& variable_declarations() const noexcept;

    // ...
};
```

* 備考
  * `sessions` リソースから取り出せる
    * `sessions` リソースそのものは `sessions_resource` として定義し、そこから参照を得られる
  * 性質上、 copy/move を禁止してもよい

### `session_container` オブジェクト

```cpp
/**
 * @brief provides living database sessions.
 */
class session_container {
public:
    /**
     * @brief registers a new session context.
     * @param session the session context to register
     * @return true if the target session is successfully registered
     * @return false if the target session is not registered
     *    because another session with such the numeric ID already exists in this container
     * @note Session labels may duplicate in this container
     */
    bool register_session(std::shared_ptr<session_context> const& session);

    /**
     * @brief find for a session context with such the numeric ID.
     * @param numeric_id the numeric ID of the target session
     * @return the corresponded session context
     * @return empty if there is no such the session
     */
    std::shared_ptr<session_context> find_session(session_context::numeric_id_type numeric_id) const;

    /**
     * @brief enumerates the list of numeric IDs of sessions in this container.
     * @return the list of numeric IDs
     * @return empty if there are no sessions in this container
     */
    std::vector<session_context::numeric_id_type> enumerate_numeric_ids() const;

    /**
     * @brief enumerates the list of numeric IDs of sessions with the specified label.
     * @param label the label text of the target sessions
     * @return the list of numeric IDs
     * @return empty if there are no sessions with such the label
     */
    std::vector<session_context::numeric_id_type> enumerate_numeric_ids(std::string_view label) const;

    // ...
};
```

* 備考
  * 性質上 copy を禁止してもよいが、利便性を考えると move は禁止すべきでない
  * reader/writer lock によってロックを取得するのが適当か
  * numeric ID で identity できるので、map 的なデータ構造で管理できそう
    * `numeric_id_type -> std::weak_ptr<session_context>`

### `session_context` オブジェクト

```cpp
/**
 * @brief Provides living database sessions.
 */
class session_context {
public:
    /**
     * @brief the numeric session ID type.
     */
    using numeric_id_type = std::size_t;

    /**
     * @brief creates a new instance.
     * @param info information about this session
     * @param variables the set of session variable
     */
    session_context(
        session_info info,
        session_variable_set variables) noexcept;

    /**
     * @brief returns the numeric ID of this session.
     * @return the numeric session ID
     */
    numeric_id_type numeric_id() const noexcept;

    /**
     * @brief returns the session label.
     * @return the label text
     * @return empty if the label is not defined for this session
     */
    std::string_view label() const noexcept;

    /**
     * @brief returns the session information.
     * @return the session information
     */
    session_info const& info() const noexcept;

    /**
     * @brief returns the session variable set.
     * @return the session variable set
     */
    session_variable_set& variables() noexcept;

    /// @copydoc variables()
    session_variable_set const& variables() const noexcept;

    /**
     * @brief returns the shutdown request status for this session.
     * @return the shutdown request status for this session
     */
    shutdown_request_type shutdown_request() const noexcept;

    /**
     * @brief requests to shutdown this session.
     * @param status the request type
     * @return true if request was accepted, or already requested the specified request type
     * @return false if request was rejected by conflict to other request
     */
    bool shutdown_request(shutdown_request_type type) noexcept;

    // ...
};
```

* 備考
  * `session show` のため、 `session_info` オブジェクトが必要
  * 上記の実装では、 `session_info` のオーナーシップを `session_context` とした
    * ライフサイクル的に一致するので基本はこの形でいいと考える
  * 「endpoint が有するセッションの情報」としてデザインしているので、必要に応じて拡張してもよさそう
    * 最終時刻を保持するようにしておけば、タイムアウトの実現も容易ではないか
      * 実際にタイムアウトする場合、 `shutdown_request()` に適切な値を設定すればよい
    * セッション固有の情報 (セッション内で開始したトランザクション等) を保持する領域をここに用意してもよい
  * オーナーシップについて
    * endpoint は `session_context` のオーナーシップを有する
      * [ライフサイクルのデザイン](#ライフサイクルのデザイン) にあるように、 `session_context` はすべてのリクエストが完了後に破棄されることになるので、 endpoint だけがオーナーシップを有すればいい
    * `session_container` は必ずしもオーナーシップを有する必要はない
      * `std::weak_ptr` を用意して、毎回 `lock()` を呼び出せばAPIは満たせるようになっている

### `shutdown_request_type` 列挙

```cpp
/**
 * @brief represents kind of shutdown request.
 */
enum class shutdown_request_type {

    /// @brief no shutdown request.
    nothing,

    /**
     * @brief safely shutdown the session.
     * @details
     * This operation terminates the session in the following order:
     *
     * 1. reject subsequent new requests and respond `SESSION_CLOSED` to them.
     * 2. wait for "complete termination" of the current request, if any
     * 3. actually terminate the session
     *
     * @note the stronger request type (e.g. shutdown_request_type::terminate) can override this request
     */
    graceful,

    /**
     * @brief terminates the session with cancelling ongoing requests.
     * @details
     * This operation terminates the session in the following order:
     *
     * 1. reject subsequent new requests, and respond `SESSION_CLOSED` to them.
     * 2. tell cancellation to ongoing requests, and  respond `SESSION_CLOSED` to them.
     * 3. wait for "complete termination" of the current request, if any
     * 4. actually terminate the session
     */
    forceful,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(shutdown_request_type value) noexcept {
    using namespace std::string_view_literals;
    using kind = shutdown_request_type;
    switch (value) {
        case kind::nothing: return "nothing"sv;
        case kind::graceful: return "graceful"sv;
        case kind::forceful: return "forceful"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, shutdown_request_type value) {
    return out << to_string_view(value);
}
```

* 備考
  * `graceful`, `forceful` の差異は、実行中のリクエストに対しキャンセル要求を前者は送らず、後者は送るのみ
  * [ライフサイクルのデザイン](#ライフサイクルのデザイン) の観点から、「即座にセッションを終了させる」という仕組みは用意しない

### `session_variable_declaration_set` オブジェクト

```cpp
/**
 * @brief declarations of session variables.
 * @details This can declare session variables used in session_variable_set.
 * @see session_variable_set
 */
class session_variable_declaration_set {
public:
    /**
     * @brief creates a new empty instance.
     */
    session_variable_declaration_set() noexcept;

    /**
     * @brief creates a new instance with initial variable declarations.
     * @param declarations the initial list of the variable declarations
     * @attention undefined behavior if some declarations has the same name
     */
    session_variable_declaration_set(std::vector<session_variable_declaration> declarations) noexcept;

    /**
     * @brief declares a new session variable.
     * @param declaration the session variable to declare
     * @return true if the variable is successfully declared
     * @return false if the variable is already declared in this set
     */
    bool declare(session_variable_declaration declaration);

    /**
     * @brief returns a previously declared session variable.
     * @param name the variable name
     * @return a pointer to the variable if it declared
     * @return nullptr if there are no such the variable with the specified name
     */
    session_variable_declaration const* find(std::string_view name) const noexcept;

    /**
     * @brief return the previously declared session variable names.
     * @return the list of declared variable names
     */
    std::vector<std::string> enumerate_variable_names() const;

    /**
     * @brief creates a new session_variable_set that holds variables declared in this object.
     * @return a new session variable set previously declared in this object
     */
    session_variable_set make_variable_set() const;

    // ...
};
```

* 備考
  * reader/writer lock を導入してもよいかもしれない
  * 引数ありのコンストラクタは conversion constructor 系の lint 警告が出ると思うので、適切に除外する

### `session_variable_declaration` オブジェクト

```cpp
/**
 * @brief represents declaration of a session variable.
 * @see session_variable_declaration_set
 */
class session_variable_declaration {
public:
    /**
     * @brief creates a new instance.
     * @param name the variable name
     * @param type the variable value type
     * @param initial_value the variable initial value, must be a suitable value type for the variable
     * @param description the description of this session variable
     */
    session_variable_declaration(
            std::string name,
            session_variable_type type,
            session_variable_set::value_type initial_value = {},
            std::string description = {}) noexcept;

    /**
     * @brief returns the variable name.
     * @return the variable name
     */
    std::string const& name() const noexcept;

    /**
     * @brief returns the variable value type.
     * @return the value type
     */
    session_variable_type type() const noexcept;

    /**
     * @brief returns the description of this session variable.
     * @return the session variable
     */
    std::string const& description() const noexcept;

    // ...
};
```

* 備考
  * `operator<<` が定義されているとよい
  * TBD: value criteria をここで定義できるようにするかどうか
    * enum 的なものをセットするには必要？

### `session_variable_type` 列挙

```cpp
/**
 * @brief represents kind of shutdown request.
 */
enum class session_variable_type {

    /// @brief the boolean value type.
    boolean,

    /// @brief the signed 64-bit integer type.
    signed_integer,

    /// @brief the unsigned 64-bit integer type.
    unsigned_integer,

    /// @brief the character string type.
    string,
};
```

* 備考
  * [`shutdown_request_type`](#shutdown_request_type-列挙) と同様に `to_string_view` やら `operator<<` を定義

### `session_variable_set` オブジェクト

```cpp
/**
 * @brief a set of session variables.
 * @see session_variable_declaration_set
 */
class session_variable_set {
public:
    /**
     * @brief the variable type.
     */
    using variable_type = session_variable_type;

    /**
     * @brief the value type.
     * @details if the value indicates std::monostate, it represents "the value is not set."
     */
    using value_type = std::variant<std::monostate, bool, std::int64_t, std::uint64_t, std::string>;

    /**
     * @brief creates a new empty instance.
     */
    session_variable_set() noexcept;

    /**
     * @brief creates a new instance.
     * @param declarations the variable declarations that consists of (variable name, variable type, initial value).
     */
    explicit session_variable_set(std::vector<std::tuple<std::string, variable_type, value_type>> declarations);

    /**
     * @brief returns whether or not the variable with the specified name exists.
     * @param name the variable name
     * @return true the variable with the name exists
     * @return false there are no variables with the name
     */
    [[nodiscard]] bool exists(std::string_view name) const noexcept;

    /**
     * @brief returns the type of the variable with the specified name.
     * @param name the variable name
     * @return the type of the target variable
     * @return empty if such the variable is not declared
     */
    [[nodiscard]] std::optional<variable_type> type(std::string_view name) const noexcept;

    /**
     * @brief return the value of the variable with the specified name.
     * @param name the variable name
     * @return the value in the target variable if it is set
     * @return std::monostate if such the variable is not declared, or the value is not set
     */
    [[nodiscard]] value_type get(std::string_view name) const;

    /**
     * @brief sets the value to the variable with the specified name.
     * @param name the variable name
     * @param value the value to set, or std::monostate to unset the variable
     * @return true if successfully set the value
     * @return false if the target variable is not declared, or the value type is not suitable for the variable
     * @see type()
     */
    bool set(std::string_view name, value_type value);

    // ...
};
```

* 備考
  * コンストラクタは適当に調整
    * `session_variable_declaration_set` をそのまま渡すと thread safety の観点であまりお勧めできない
  * 変数の増減をこのオブジェクト上で行わない
  * `tgctl session set` と `session_variable_set::{get,set}` がかち合うため、ガードが必要
    * `value_type` は `std::string` が混ざっているため、 not trivially copyable であり、 `std::atomic` は利用できない
    * reader/writer lock をコンテナ全体か、個々の変数に対して用意する必要がある

## Java API

* 提供しない

## Protobuf

### SessionInfo

* `session show` の出力に必要十分な情報を提供する
  * `session list` でも同様のデータ構造を利用する

```proto
// the running session information.
message response.SessionInfo {
    // the session ID, starting with ":".
    string session_id = 1;

    // the session label (may be empty).
    string label = 2;

    // the application name (may be empty).
    string application = 3;

    // the session user name (may be empty).
    string user = 4;

    // the session starting time (millisecond offset from 1970-01-01T00:00:00.000Z).
    sfixed64 start_at = 6;

    // the session connection type (e.g. "ipc" or "tcp").
    string connection_type = 6;

    // the remote host information (e.g. TCP: remote host, IPC: PID).
    string connection_info = 7;
}
```

### ErrorInfo

`tgctl session` サブコマンド共通のエラー情報。

```proto
// the error code.
enum dianostic.ErrorCode {

    // the error code is not set.
    ERROR_CODE_NOT_SPECIFIED = 0;

    // the unknown error was occurred.
    UNKNOWN = 1;

    // the target session is not found.
    SESSION_NOT_FOUND = 2;

    // the session label is not unique within the target sessions.
    SESSION_AMBIGUOUS = 3;

    // the operation was not permitted.
    OPERATION_NOT_PERMITTED = 4;

    // the target session variable is not declared.
    SESSION_VARIABLE_NOT_DECLARED = 5;

    // the setting value is invalid for the target variable.
    SESSION_VARIABLE_INVALID_VALUE = 6;
}

// the error information of the session control operations.
enum diagnostic.Error {

    // the error code.
    ErrorCode error_code = 1;

    // the error message (optional).
    string message = 2;

    // the error message code (optional).
    string message_code = 3;

    // the arguments for the message code (optional).
    repeated string message_arguments = 4;
}
```

* 備考
  * 拾いきれていないエラーがありそうなので、適宜追加のこと

### SessionGet

* `session show` に対応

```proto
// obtains session information.
message request.SessionGet {

    // the target session specifier - ID or label.
    string session_specifier = 1;
}

// the results of request.SessionGet.
message response.SessionGet {
    // the successful message.
    message Success {
        // the obtained session information.
        SessionInfo entry = 1;
    }

    // the response body.
    oneof result {
        // request is successfully completed.
        Success success = 1;

        // request was failed by error.
        diagnostic.Error error = 2;
    }
}
```

### SessionList

* `session list` に対応
  * 現在の権限で表示可能なもののみを表示

```proto
// obtains list of active sessions.
message request.SessionList {
    // no special properties.
}

// the results of request.SessionList.
message response.SessionList {

    // the successful message.
    message Success {
        // the obtained list of sessions.
        repeated SessionInfo entries = 1;
    }

    // the response body.
    oneof result {
        // request is successfully completed.
        Success success = 1;

        // request was failed by error.
        diagnostic.Error error = 2;
    }
}
```

### SessionShutdown

* `session shutdown` に対応

```proto
// represents kind of shutdown request.
enum request.SessionShutdownType {

    // the shutdown request type is not set (default behavior).
    SESSION_SHUTDOWN_TYPE_NOT_SET = 0;

    // denies new requests, and shutdown the session after the all running requests were finished.
    GRACEFUL = 1;

    // denies new requests, tells cancellation to the running requests,
    // and then shutdown the session after the all requests were finished or cancelled.
    FORCEFUL = 2;
}

// request shutdown to the session.
message request.SessionShutdown {

    // the target session specifier - ID or label.
    string session_specifier = 1;

    // the shutdown request type.
    SessionShutdownType request_type = 2;
}

// the results of request.SessionShutdown.
message response.SessionShutdown {

    // the successful message.
    message Success {}

    // the response body.
    oneof result {
        // the shutdown was successfully requested.
        Success success = 1;

        // request was failed by error.
        diagnostic.Error error = 2;
    }
}
```

* 備考
  * `session shutdown` を実行する際、 `request.SessionShutdown.request_type` は unset のまま送る
    * サービス側で `SessionShutdownType.FORCEFUL` として処理する

### SessionSetVariable

* `session set` に対応

```proto
// set session variables.
message request.SessionSetVariable {

    // the target session specifier - ID or label.
    string session_specifier = 1;

    // the target variable name, case insensitive.
    string name = 2;

    // the value to set, or NOT_SET to unset the variable.
    oneof value_opt {
        // the text represented value to set.
        string value = 3;
    }
}

// the results of request.SessionSetVariable.
message response.SessionSetVariable {

    // the successful message.
    message Success {}

    // the response body.
    oneof result {
        // the session variable was successfully set.
        Success success = 1;

        // request was failed by error.
        diagnostic.Error error = 2;
    }
}
```

### SessionGetVariable

* 対応コマンドなし

```proto
// retrieve session variables.
message request.SessionGetVariable {

    // the target session specifier - ID or label.
    string session_specifier = 1;

    // the target variable name, case insensitive.
    string name = 2;
}

// the results of request.SessionSetVariable.
message response.SessionSetVariable {

    // the successful message.
    message Success {

        // the canonical variable name.
        string name = 1;

        // the description of the variable.
        string description = 2;

        // the resulting value, or NOT_SET if the value is empty.
        oneof value {

            // the boolean value.
            bool bool_value = 3;

            // the signed integer value.
            sint64 signed_integer_value = 4;

            // the unsigned integer value.
            uint64 unsigned_integer_value = 5;

            // the character string value.
            string string_value = 6;
        }
    }

    // the response body.
    oneof result {
        // the session variable was successfully set.
        Success success = 1;

        // request was failed by error.
        diagnostic.Error error = 2;
    }
}
```
