# transactional kvs resource

2024-01-31 kurosawa

## 当文書について

`transactional_kvs_resource` クラスについて開発者向けに情報共有するための資料

## `transactional_kvs_resource` クラス

`transactional_kvs_resource` はsharksfinをtateyamaのリソースコンポーネントとするためのクラスである

これにより、起動・停止等のライフサイクル管理を `tateyama::framework` に任せることができる

 (リソース、コンポーネント等の用語については [tateyama-arch-ja.md][Arch] を参照 )


起動時に構成ファイル `tsurugi.ini` の内容を適切な初期化オプションとして下位モジュール(sharksfin, shirakami)に中継する役割も担うため、下位モジュールに関連する構成パラメータの追加変更がある場合はこのクラスも修正変更する必要がある

## 起動時の挙動

`tateyama::framework` が行うコンポーネント起動の仕様は [tateyama-arch-ja.md][Arch] の通りだが、ここでは起動時の実装概要を紹介する

0. `tateyama-bootstrap` の `backend_main` ( `main` から実行される)でtsurugi構成ファイル(tsurugi.ini)が読み込まれその内容が `conf` に保持される

    https://github.com/project-tsurugi/tateyama-bootstrap/blob/0bb4d5b04c439105bcafd83967f101fbebc83880/src/tateyama/server/backend.cpp#L187

1. `conf` を渡して `tateyama::framework::server` が作成され、`tateyama::framework::server::add_core_components()` で `transactional_kvs_resource` がフレームワークのコアコンポーネントの一つとして初期化され追加される

    https://github.com/project-tsurugi/tateyama-bootstrap/blob/0bb4d5b04c439105bcafd83967f101fbebc83880/src/tateyama/server/backend.cpp#L241

2. `tateyama::framework::server::setup()` が呼ばれ、その中で`transactional_kvs_resource::setup()` も実行される

    https://github.com/project-tsurugi/tateyama-bootstrap/blob/0bb4d5b04c439105bcafd83967f101fbebc83880/src/tateyama/server/backend.cpp#L258

    ここで構成パラメーター(`conf`の内容)を抽出し `sharksfin::DatabaseOptions` を作成し、sharksfinを初期化する。sharksfinの延長でshirakami、その延長でlimestoneも初期化される。(`shirakami::init()`の処理)

3. `tateyama::framework::server::start()` が呼ばれ、その中で`transactional_kvs_resource::start()` も実行されるが、現実装ではDBの初期化は上記setupで実行され、startの方では得に何もしていない。

    https://github.com/project-tsurugi/tateyama-bootstrap/blob/0bb4d5b04c439105bcafd83967f101fbebc83880/src/tateyama/server/backend.cpp#L275

## 停止時の挙動

1. `tateyama::framework::server::shutdown()` が呼ばれ、その中で`transactional_kvs_resource::shutdown()` も実行される

    https://github.com/project-tsurugi/tateyama-bootstrap/blob/0bb4d5b04c439105bcafd83967f101fbebc83880/src/tateyama/server/backend.cpp#L311

    `sharksfin::database_close()`でsharksfinを停止し、`shirakami::fin()`が実行される。
    `sharksfin::database_dispose()`で不要なインスタンスが削除される。

## 起動時のモードについて

tsurugidbのプロセスには複数の起動時モードがあり(定義は[tateyama-arch-ja][Arch]を参照) `framework::boot_mode` によって渡される。`transactional_kvs_resource` は起動モードに応じて適切な挙動になるよう下位のモジュール(sharksfin, shirakami, limestone等)に初期化オプション等を渡す必要がある。

- `database_server`
通常の起動モード。トランザクションエンジンは必要であればリカバリ等を初期化時に行う

- `maintenance_server`, `maintenance_standalone`
リストアのためのメンテナンスモード。
トランザクションエンジンはlimestoneにアクセス可能にする以外の機能は期待されない

- `quiescent_server`
静止状態で起動するモード(主にバックアップのため) 通常の起動を行うが停止以外の要求を受けることはない

## その他

- 現実装ではshirakamiによってlimestoneが初期化され保持されているが、本来はlimestoneも独立したtateyamaのリソースとして初期化されることが望ましい
  - この事情のため、limestone向けの構成パラメータも一旦 `transactional_kvs_resource` が構成ファイルから読み出して渡すという回り道になっている
  - maintenanceモードでlimestoneのみ初期化を行うのも同じ理由であり、shirakamiの初期化を経由せずにlimestoneの初期化が実施されるのが望ましい

## 参考情報

[tateyama-arch-ja][Arch]

[Arch]: https://github.com/project-tsurugi/tateyama/blob/master/docs/tateyama-arch-ja.md
