# Tateyama message routing architecture

2022-04-12 arakawa (NT)

## この文書について

* tateyama が複数アプリケーションをホストできるように、デザインの骨格を検討する
  * 各種名前や疑似コードは初案なので、必要に応じて変更されたし

## 基本的な考え方

* リソースとサービスを分けて管理する
  * リソースは、tateyama 内のアプリケーションが横断的に利用するコンポーネント
  * サービスは、アプリケーションごとにメッセージを受け取って処理するコンポーネント
* コンポーネントライフサイクルを重視する
  * resource, service, end-point はいずれも component
  * 各 component は、以下の lifecycle を持つ
    * `initial` - 初期状態
    * `ready` - 準備状態 (設定反映後)
    * `activated` - 稼働状態
    * `deactivated` - 非活性化状態
    * `disposed` - 破棄状態 (オブジェクト消滅)
  * 上にある状態から下にある状態に遷移し、逆はない
  * 基本的には1つずつ順に遷移するが、飛ばして遷移する場合もある
  * 可能な限り全てのコンポーネントのライフサイクルをそろえて遷移させる
  * 全てのコンポーネントが deactivated になってから終了した場合、安全に終了したといえる
* サービスモードを新設する
  * モードごとに利用可能なリソースやサービスが存在する
  * モードは以下
    * `database_server` - 通常のデータベースとして起動
    * `database_standalone` - 定義しない
    * `maintenance_server` - 保守モードかつサーバーとして起動
    * `maintenance_standalone` - 保守モードかつスタンドアロンアプリケーションとして起動
    * `quiescent_server` - 静止モードかつサーバーとして起動
    * `quiescent_standalone` - 定義しない
  * モードごとに各コンポーネントの `activated` フェーズにおいて提供する機能が異なる
  * モードごとのコンセプトは以下
    * 通常系 (`database_*`) - あらゆる機能を利用できる
    * メンテナンス系 (`maintenance_*`) - システム管理機能のみを利用できる
      * DBA的な管理 (スキーマ変更等) **ではない**
    * 静止系 (`quiescent_*`) - あらゆる機能が利用できない
      * 例外的に、ステータス確認とシャットダウンは利用可能

## 登場人物

* resource
  * 各アプリケーションが横断的に利用するコンポーネント
    * アプリケーションに閉じたコンポーネントは resource として取り扱う必要はない
  * 例: sharksfin, task scheduler, status watcher, auth component, etc.
* service
  * 各アプリケーションのロジックを構成するコンポーネント
  * 利用者からのコマンドを受け取り、その内容に従って処理を行う
  * 例: sql service (jogasaki), status service, backup service, etc.
* end-point
  * 利用者と通信を行い、アプリケーションのコマンドや結果を授受するコンポーネント
  * 例: ipc end-point, tcp/ip end-point
* router
  * end-point と service の仲介を行い、二者間でメッセージの授受を行うコンポーネント
  * 単なる service の一種 (`routing_service`)

## class design

### environment

```c++
// namespace tateyama::framework
class environment {
public:
    mode mode();
    api::configuration const& configuration();
    repository<resource>& resource_repository();
    repository<service>& service_repository();
    repository<endpoint>& endpoint_repository();
}
```

### repository

```c++
// namespace tateyama::framework (?)
template<class T>
class repository {
public:
    void each(std::function<void(T&)>)
    void add(std::shared_ptr<T>);
    T* find_by_id(component::id_type);

    template<class U>
    U* find() {
        static_assert(std::is_same_type_v<decltype(U::tag), component::id_type>);
        auto* c = find_by_id(U::tag);
        return static_cast<U*>(c);
    }
}
```

* コメント
  * bare pointer/reference を利用しているが、安全のため shared ptr にした方がいいかもしれない
    * wire のコストで atomic increment のコストが隠れるはずなのでこのくらいいいかな？

### component

```c++
// namespace tateyama::framework
class component {
public:
    using id_type = std::uint32_t;
    static constexpr id_type max_system_reserved_id = 255;
    component(); // -> initial
    virtual void setup(environment&) = 0; // -> ready
    virtual void start(environment&) = 0; // -> activated
    virtual void shutdown(environment&) = 0; // -> deactivated
    virtual ~component() = default; // -> disposed

    virtual std::string_view label() const noexcept = 0;
    virtual std::vector<std::string> configuration_sections() = 0;
}
```

* コメント
  * `configuration_sections()` はこのコンポーネントが支配する設定のセクション名
    * 誰も支配下に置いていないセクションがあればエラー表示するなどを想定
    * configuration オブジェクト側に診断用の項目を付けてもいいが、こちらでもよさそう
    * きれいなやり方が他にあれば求む

#### resource

```c++
// namespace tateyama::framework
class resource : public component {
public:
    virtual id_type id() const noexcept = 0;
};
```

* コメント
  * `id()` は resource 間でユニークであればいい
  * `label()` は英数とアンダースコアからなる文字列で、厳密なユニーク性は要求しない (人間用)
  * resource は基本的に具象型を把握したうえで利用するため、この抽象クラスに特別な機能はない

#### service

```c++
// namespace tateyama::framework
class service: public component {
public:
    virtual id_type id() const noexcept = 0;

    virtual void operator()(
        std::shared_ptr<request> req,
        std::shared_ptr<response> res) = 0;
};
```

* コメント
  * `id()` は service 間でユニークであればいい
  * `label()` は英数とアンダースコアからなる文字列で、厳密なユニーク性は要求しない (人間用)
  * service はメッセージを受け取る口がある

#### end point

```c++
// namespace tateyama::framework
class endpoint : public component {};
```

* コメント
  * end point は今のところ識別する必然性がないため ID がない
    * 必要ならば別途用意する
    * 入れた方が `repository` の API を統一できて楽だが、IDを重複なく降るという苦行が待ち受ける
  * `label()` は英数とアンダースコアからなる文字列で、厳密なユニーク性は要求しない (人間用)
  * end point は能動的に動作するため、この抽象クラスに特別な機能はない

#### router

```c++
// namespace tateyama::framework (?)
class routing_service : public service {
public:
    static constexpr id_type tag = ...;

    id_type id() const noexcept override {
        return tag;
    }

    std::string_view label() override const noexcept {
        return "...";
    }

    void setup(environment& env) override {
        services_ = std::addressof(env.service_repository());
    }

    void start(environment&) override {}
    
    void shutdown(environment&) override {
        services_ = {};
    }

    void operator()(
            std::shared_ptr<request> req,
            std::shared_ptr<response> res) override {
        // 無限ループを防ぐ
        if (req->service_id() == tag) {
            throw ...;
        }

        auto* svs = services_;
        if (svs == nullptr) { // 初期化エラー
            throw ...;
        }

        // 宛先の service ID に対し chain する
        if (auto* destination = svs.find_by_id(req->service_id());
                destination != nullptr) {
            destination->operator()(std::move(req), std::move(res));
            return;
        }

        // 宛先エラー
        res->...;
    }

private:
    repository<service>* services_ {};
};
```

* コメント
  * router は service の一種で、他の service にメッセージを転送する役割
    * 同様に、 service 間でメッセージをやり取りすることは問題なし
  * 現在決め打ちでjogasakiを呼び出している箇所 ([invoke service]) を決め打ちで `routing_service` を呼び出すようにすればいい
    * `environment.service_repository().find<routing_service>()` で取得可能
    * `routing_service` は core component として必ず利用可能とする
  * 宛先の `service_id` が新たに必要になるため、ヘッダ部に追加する
    * 32-bit unsigned int くらいで十分だと思うが、面倒なら文字列という手もある
    * `routing_service` 自体の `service_id` は privileged ID
  * 現在は `repository` 経由でサービスを引っ張っているが、手元にコピーを作ってしまってもよい
    * `setup()` の段階でオブジェクト自体は出そろっている

### server

```c++
// namespace tateyama::framework
class server {
public:
    explicit server(api::configuration&& conf, framework::mode);

    void add_resource(std::shared_ptr<resource>);
    void add_service(std::shared_ptr<service>);
    void add_endpoint(std::shared_ptr<endpoint>);

    resource& get_resource_by_id(component::type_id_type); // throws not_found
    template<class T> get_resource(); // throws not_found

    void start(); // call .setup(), .start() for each components
    void shutdown(); // call .shutdown() for each components
};
```

* コメント
  * `tateyama-bootstrap` から操作するための facade
    * `add_*` はコンポーネント登録用
    * `get_*` は standalone 時に resource を直接取得するための仕組み
      * `service` の方を取得してロジックをそちらに書いてもよい
    * `start, shutdown` は主題のライフサイクル管理
  * reference で返している部分は `std::shared_ptr` で返す方が適切かもしれない
    * 対した回数呼び出されないので
  * 具体的な棲み分けは [module role](#module-role) を参照のこと

## behavior

### lifecycle status

* ライフサイクル上の各フェーズは以下のような状態を想定している
  * `initial`
    * C++ オブジェクトが作成された初期状態
    * 設定情報の読み出し前で、基本的にこの時点でエラーになることは想定していない
  * `ready`
    * 設定情報が読みだされ、コンポーネントに反映された状態
    * コンポーネントの稼働前なので機能の大半を利用できないが、メモリーなどの計算機上のリソースをあまり消費していない状態
  * `activated`
    * 稼働状態
    * モードによって動作内容が異なる
  * `deactivated`
    * 稼働終了状態
    * コンポーネントに関するすべての計算機上のリソースが正しく解放されている
  * `disposed`
    * C++ オブジェクトが削除された終了状態
    * この状態への推移は可能な限り素早く、かつ例外をスローしない形で行われるべきである
* 備考
  * `ready` と `activated` を分けているのは、 component がすべて `ready` になるまで `activate` を開始したくないため
    * 依存関係の解決や、中途半端な状態のエラーを可能な限り低減させる
    * 実装上面倒だったら、 `ready` に推移する際には何もせず、 `activate` に移行する際に全ての初期化処理を行えばいい
  * 上記をうまく RAII 化するとすると、以下のようなデザインが考えられる
    * `service` などを直接継承するクラス (以下、bridge) は、内部にサービスの実体 (以下、core) を unique ptr 等で持つ
    * bridge は initial の状態では core を instantiate しておらず、 ready または activated で instantiate する
    * bridge は deactivated で core のインスタンスを安全に破棄する
    * bridge は disposed で core のインスタンスが残っていたら、素早い方式で破棄する
    * bridge が resource の場合、 core を暴露する機能を提供する
    * bridge が service の場合、 `operator()` の呼び出しを core に委譲する
  * とにかく、 bridge は core のライフサイクル管理以外の役割を持たないようにすることが様々な面で重要である
    * `environment` が god object 化しているため、依存関係が fat な bridge, slim な core と分けて管理しないと破綻しかねない
    * 特に、きれいに分けてテストの対象は core を中心にするなどが必要
  * 現行の実装 **とは別に** `framework::{resource,service,endpoint}` を継承したクラスを用意し、現行の実装を内包するとよい
    * まずは `setup()` で現行のインスタンス生成、 `start()` で開始、 `shutdown()` で終了するように変更する
    * 現行の実装でライフサイクル制御に関する部分を component 側に移す
    * 最終的に、現行の実装から `framework` 名前空間への参照がなくなると完璧
      * 多少残っていてもいいが、 `framework` 部分はライフサイクル制御に関する箇所が主なので、 `framework` 以外から他者のライフサイクル制御をするのは筋が違う

### initialization order

* 各 component は以下の順に高い優先度が付けられる
  1. resource
  2. service
  3. end point
* 各ライフサイクルへの推移は以下の順序で行われる
  * `-> ready` - 優先度が高いものから順に
  * `-> activated` - 優先度が高いものから順に
  * `-> deactivated` - 優先度が低いものから順に
  * `-> disposed` - 優先度が低いものから順に
* なお、resource, service, end point 内でも優先順位を決めておく
  * 例えば、 sharksfin とそれを decorate した resource がある場合、前者の優先度を高く設定する必要がある
  * これは、「 `repository` に追加した順に優先度が高い」などの規則を決めておけばよい
* これらの規則により、 `service`, `endpoint` に先立って `resource` の準備が整っていることが保証される
* なお、tateyamaでは component 間の細かな **依存関係の制御を行わない**
  * component A から B への依存がある場合、 B を A より高い優先度に配置することで、先に初期化処理が行われていることを保証する

### service provider interface

* コンポーネントを外部から追加するために、service provider interface (SPI) を用意する
* 以下のようにコンポーネントを追加する
  * 以下の関数を提供する `.so` を作成する (必要なものだけ用意すればいい)
    * `extern "C" void create_resources(std::function<void(std::shared_ptr<resource>)>);`
    * `extern "C" void create_services(std::function<void(std::shared_ptr<service>)>);`
    * `extern "C" void create_endpoints(std::function<void(std::shared_ptr<endpoint>)>);`
  * 上記 `.so` ファイルを特定のディレクトリに配備する
  * 設定ファイルに上記ディレクトリの位置を記載する (`libexec/tateyama-x.y.z/components` 辺り?)
* 上記コンポーネントは `dlopen()`, `dlsym()` 経由で入手できる

### bootstrap mode

* 起動時のモードごとに各コンポーネントの `activated` 状態の挙動が異なる

  | 項目 | `database_server` | `maintenance_server` | `maintenance_standalone` | `quiescent_server` |
  |---|:-:|:-:|:-:|:-:|
  | データベース   | ○ | △ (保守モード) | △ (保守モード) | x |
  | スケジューラ   | ○ | × | × | x |
  | 認証機構       | ○ | ○ | ○ | o |
  | サービス       | ○ | ○ | × | △ (起動制御のみ) |
  | エンドポイント | ○ | △ (一部のみ) | × | △ (一部のみ) |
  | ルーター       | ○ | ○ | × | ○ |

* 備考
  * "×" となっている部分は、そもそもコンポーネントを `activated` にする必要性はないが、依存解決がめんどくさそうなので「`activated` だがまともに働いていない」という状態がよさそう
  * `maintenance_standalone` での実行時にも外部からステータスの取得や shutdown の要求を受け付ける必要がある場合、そもそも `maintenance_server` で稼動させるか、または `maintenance_standalone` でも各種通信経路を有効にする必要がある
  * `quiescent_server` はステータスの取得や shutdown の要求以外を拒否する
    * 無関係のサービスへのコマンドは利用不可能
    * 排他的に静止状態を作り出すためのモードで、主に静止状態のバックアップ作成に利用する

### bootstrap sequence

```sh
tateyama-bootstrap \
    [--conf </path/to/conf>] \
    <behavior-options>
    ...

<behavior-options>:
    [--database] |
    --maintenance |
    --quiescent [--message <message>] |
    --restore-backup </path/to/backup> [--keep-backup|--no-keep-backup] |
    --restore-tag <tag-name>
```

* `behavior-options` に指定された値に対して、それぞれ以下のモードで起動する

  | オプション名 | 起動するモード |
  |---|---|
  | 未指定 | `database_server` |
  | `--database` | `database_server` |
  | `--maintenance` | `maintenance_server` |
  | `--quiescent` | `quiescent_server` |
  | その他 | `maintenance_standalone` |
* 各モードについて、それぞれ以下のように実行する
  * 初期化
    * 各コンポーネントのオブジェクトを生成する (SPI経由含む)
  * 準備
    * 各コンポーネントを `-> ready -> activate` の順に遷移させる
  * 本体
    * `*_server` モード
      * `SIGINT`, `SIGTERM` が来るまでポーリングを行う
    * `maintenance_standalone` モード
      * それぞれ以下の関数を実行する

        | オプション名 | 実行する関数 |
        |---|---|
        | `--restore-backup` | `restore_backup(framework::server&, std::string_view, bool)` |
        | `--restore-tag` | `restore_tag(framework::server&, std::string_view)` |

        * それぞれの関数は `tateyama-bootstrap` 内に作成 ([module role](#module-role) を参照)

  * 終了処理
    * 各コンポーネントを `-> deactivate -> disposed` の順に遷移させる
* 備考
  * `--restore-tag` などは `maintenance_server` で動かしてクライアントからコマンドで指示を出してもよい
  * 逆に `--tag-{list,add,remove,show}` なども `maintenance_standalone` として bootstrap 内に作りこんでもよい
  * 性能や保守性、初動開発の効率を見て適宜判断されたし
  * `--quiescent --message "something"` は、ステータス確認が行われた際に表示するメッセージを想定
    * 「Aがバックアップを作成中」などのイメージ
    * 後回しでもいい

### module role

* `tateyama`
  * コンポーネントライフサイクルを管理するフレームワーク
  * コンポーネントに resource, service, end-point があるという事実は知っている
  * それぞれのコンポーネントの種類や詳細は知らない
  * リクエストに応じて各コンポーネントのライフサイクルの遷移を行う
    * `framework::server.{start,shutdown}()` があるイメージ (`start` は `setup & start`)
    * これが呼び出されると、個々のコンポーネントのライフサイクル制御を行う
* `tateyama-bootstrap`
  * 上記 `framework::server` を構築し、同オブジェクトのライフサイクルを管理するアプリケーション
    * `framework::server.{start,shutdown}()` を呼び出す側
  * `framework::server` へ個々のコンポーネントの登録と、その際に優先順位の規定を行う
    * その関係で、コマンドラインオプションの解析や、設定ファイルの読み出しも bootstrap 側で行う
  * 個々のコンポーネントのライフサイクル制御は行わず、 `framework::server` を通して行う
  * 各モードにおける本体処理 (signal polling, etc.) は bootstrap 側に記述する
    * ライフサイクル管理の範疇ではないため、bootstrap 側の作業
    * `maintenance_standalone` モードの際、 `framework::server` から個々のコンポーネントを取得し、直接操作するケースがある

## その他

### process mutex

* データベースは、同じ設定ファイルを参照するプロセスを同一コンピューター上に複数稼働させることはできない
* 以下のように実現する
  * configuration オブジェクトを構成する際に、その識別情報 (canonical path や hash など) を別途保持しておく
  * process mutex を表す resource を repository に追加する
    * [ready, deactivated] の区間で mutex を保持する
    * mutex は、 configuration オブジェクトの識別情報をもとにした named socket を作ったり、特定位置にファイルを作成して file lock をとるなどが考えられる
* また、上記の mutex は CLI を開発する上でも利用できる
  * CLI では、設定ファイルへの参照の位置異性を利用して、設定ファイルから稼動中のデータベースプロセスを検出する必要がある
  * mutex として作成したソケットやファイル経由で対象の PID を取得できれば、 kill を行う際に役に立つ
  * また、mutex が獲得されていることを確認出来たら、対象の設定ファイルに記載された通信路を利用して、サーバーとの通信を行える

### session lifecycle management

* やりたいこと
  * セッションの終了とともに、関連する全てのリソースを開放する
* 問題点
  * セッションの終了に関するイベントがない
  * イベントがあったとしても、複数の service にまたがって処理する場合があるため通知が難しい
  * リース方式ではリソース開放までにタイムラグがあり、無駄が多い
* 疑問点
  * 基本的にクライアントが注意深く開放すればいいのではないか
  * クライアントが死んだらどちらにしろリース方式頼みだから特段処理する必要はないのでは
* 解決案
  * end point
    * セッションの開始と終了時に、 session storage (is a resource) に対して開始/終了を通知する
    * セッション内でメッセージを受け取るたびに、 session storage に生存を通知する
  * services
    * アプリケーション内でセッション固有の情報を保持する際に、 session storage に shared ptr を追加
    * リクエストの処理中に、定期的に session storage に「生存」を通達
    * 以降、セッション固有の情報は session storage から shared ptr を取得して利用する
  * session storage
    * 「開始」を受け取ったらマップに対象セッション用のエントリを作成
    * 「生存」を受け取ったら対象エントリのリース期限を更新
    * 「終了」を受け取ったら、関連する shared ptr を開放 (または終了処理へのコールバックを起動？)
    * リース期限が切れたエントリがあれば「終了」扱いで処理
    * 「終了」後に shared ptr を要求されたらリースエラー
* 備考
  * resource leasing と合わせ技でやってしまえば多少楽か？
  * どのくらい衝突するかわからないので、場合によっては sharding by session id など？
