# Tsurugi OLTP CLI 仕様 (案)

2022-04-11 arakawa (NT)

## この文書について

* Tsurugi OLTP の外部コマンド案
  * front-end とは別で、OLTP部分のサービスだけ記載

---
NOTE: for developers

* impl memo に実装の流れ
* 内部コマンドは [Tateyama message routing architecture] に準拠
* target service name は [Tateyama component 一覧] に準拠
* OLTP サービスプロセスを単に service と呼称
* service status を以下に分類
  * present - 通信可能
  * absent - mutex がない状態
  * unknown - mutex があるが、通信不可能

[Tateyama message routing architecture]:20220412-tateyama-arch.md
[Tateyama component 一覧]:20220415-tateyama-components.md

## コンセプト

* ユーザーが利用するコマンドを一つにまとめ、サブコマンドですべての機能を提供する
  * そこから `exec` などで真のコマンドを呼び出したり、 python などのスクリプトを起動したりしてもよい
  * メインコマンドを `/bin` へ、それ以外の executable は `/libexec` 以下に配置
  * メインコマンドのバイナリ名を仮に `oltp` とおく
* データベースの設定パスごとに、高々ひとつのデータベースプロセスを起動できる
  * 複数のデータベースプロセスを稼働させたい場合、異なる設定ファイルを用意する必要がある
  * 異なる設定ファイルであっても、設定ファイル間で競合するリソース (e.g. WAL出力先ディレクトリ等) が存在する場合、複数を同時に稼働できない
* front-end との連携を行う場合、front-end と OLTP を統括するコマンドを別途用意すること

## CLI

### 起動・終了

```sh
oltp start [--conf </path/to/conf>] [--recovery|--no-recovery]
oltp shutdown [--conf </path/to/conf>]
oltp kill [--conf </path/to/conf>]
oltp status [--json] [--conf </path/to/conf>]
```

* overview
  * データベースのライフサイクルを制御する
* 共通
  * options
    * `--conf` - 設定ファイルのパス、未指定の場合は既定の設定パスを利用
* サブコマンド
  * `start`
    * overview
      * データベースプロセスを起動する
    * options
      * `--recovery` (default) - データベース起動時にリカバリ操作が必要であれば自動的に行う
      * `--no-recovery` - データベース起動時にリカバリ操作が必要であれば何もせずにエラー終了する
    * note
      * 同一の設定ファイルを参照するデータベースが稼働中である場合、コマンドは失敗する
      * 同一の設定ファイルを参照するデータベースが完全に終了していない場合、コマンドは失敗する場合がある
    * impl memo
      * if service is absent
        * exec service with `--database`
        * wait until service is ready
      * otherwise
        * raise error
  * `shutdown`
    * overview
      * データベースプロセスを安全に終了させる
    * options
      * (N/A)
    * note
      * 同一の設定ファイルを参照する、稼働中のデータベースを対象にとる
      * 対象のデータベースが未稼働の状態でこのコマンドを実行した場合、コマンドは失敗する場合がある
      * 対象のデータベースが安全かつ完全に終了した場合のみ、このコマンドの exit status は 0 になる
      * 対象のデータベースが安全に終了するか、安全な終了に失敗するまで制御を返さない
    * impl memo
      * if service is ready
        * send `shutdown` to "control_service"
        * wait until service is absent
      * if service is absent
        * do nothing
      * if service is unknown
        * raise error
  * `kill`
    * overview
      * データベースプロセスを完全に終了させる
    * options
      * (N/A)
    * note
      * 同一の設定ファイルを参照する、稼働中または完全に終了していないデータベースを対象にとる
      * 対象のデータベースがどのような状態であっても完全に終了する
        * 安全に終了させるには、 `shutdown -> kill` の順に実行する
      * 対象のデータベースが完全に終了している状態でこのコマンドを実行しても、何も起こらない
    * impl memo
      * if service is absent
        * do nothing
      * otherwise
        * try to kill the service
  * `status`
    * overview
      * データベースの状態を表示する
    * options
      * `--json` コマンドをJSON形式で出力する
    * note
      * 同一の設定ファイルを参照する、未稼働または稼働中のデータベースを対象にとる
      * 対象のデータベースが未稼働の場合、このコマンドはその旨を表示する
      * 対象のデータベースが稼働中の場合、コマンドは各機能の状態を表示する
      * `--json` は一例で、machine readable なフォーマットが一つあればいい
        * それらのオプションがなにも指定されない場合、human readable な形式で出力する
    * impl memo
      * if service is present
        * send `status` to "control_service"
      * if service is absent
        * tell it is absent
      * if service is unknown
        * tell it is unknown

### backup サブコマンド

```sh
oltp backup create </path/to/backup> [--conf </path/to/conf>] [--overwrite] [-v|--verbose]
oltp backup estimate [--conf </path/to/conf>]
```

* overview
  * データベースのバックアップを作成する
* 共通
  * options
    * `--conf` - 設定ファイルのパス、未指定の場合は既定の設定パスを利用
  * note
    * 同一の設定ファイルを参照する、稼働中または未稼働のデータベースを対象にとる
    * 対象のデータベースが未稼働の場合、コマンドの終了まで一時的にデータベースをメンテナンスモードで起動する
      * メンテナンスモードでは、データベースプロセスを起動しているが利用可能な機能が大幅に制限されている
      * 作業完了後、データベースプロセスは完全に終了する
* サブコマンド
  * `create`
    * overview
      * 現在のデータベースの状態を複製し、バックアップファイルとして書き出す
    * options
      * `</path/to/backup>` - バックアップ先のディレクトリ
      * `--overwrite` - 出力先にディレクトリが既に存在していた場合、削除してからバックアップファイルを出力する
      * `-v,--verbose` - 詳細情報を表示する
    * note
      * 作成したバックアップは `oltp restore backup` で復元できる
      * 出力先に空でないディレクトリが既に存在し、かつ `--overwrite` が指定されていない場合、コマンドは失敗する
    * impl memo
      * if service is present
        * send `backup_begin` to "backup_service"
        * copy files
          * send `keep_alive` to "control_service" periodically for lease update
        * send `backup_end` to "backup_service"
      * if service is absent
        * exec service with `--maintenance`
        * process as it was present
        * send `shutdown` to "control_service"
        * wait until service is absent
      * if service is unknown
        * raise error
  * `estimate`
    * overview
      * バックアップファイルの推定サイズを表示する
    * options
      * (N/A)
    * impl memo
      * if service is present
        * send `backup_estimate` to "backup_service"
      * if service is absent
        * exec service with `--maintenance`
        * process as it was present
        * send `shutdown` to "control_service"
        * wait until service is absent
      * if service is unknown
        * raise error
* note
  * データベースが未稼働の場合、全てのWALとデータストアの出力ファイルをコピーすることでもバックアップファイルとして利用できる
    * ファイル名のパターンによってファイルの種類を識別しているため、元のファイル名を変更しなければディレクトリ構成は自由に変更してよい
    * これらのファイルは複数のディレクトリは以下にまたがっている場合があるが、リストア時には単一のディレクトリは以下にまとまっている必要がある

### restore サブコマンド

```sh
oltp restore backup </path/to/backup> [--conf </path/to/conf>] [--keep-backup|--no-keep-backup] [-f|--force]
oltp restore tag <tag-name> [--conf </path/to/conf>] [-f|--force]
```

* overview
  * データベースを特定の状態に復元する
* 共通
  * options
    * `--conf` - 設定ファイルのパス、未指定の場合は既定の設定パスを利用
  * note
    * 同一の設定ファイルを参照する、完全に終了しているデータベースを対象にとる
    * 対象のデータベースが完全に終了していない場合、このコマンドは失敗する
    * 対象のデータベースが未稼働の場合、コマンドの終了まで一時的にデータベースをメンテナンスモードで起動する
      * メンテナンスモードでは、データベースプロセスを起動しているが利用可能な機能が大幅に制限されている
      * 作業完了後、データベースプロセスは完全に終了する
* サブコマンド
  * `backup`
    * overview
      * バックアップファイルからデータベースをリストアする
    * options
      * `</path/to/backup>` - バックアップファイルの位置
      * `--keep-backup` (default) - 入力したバックアップファイルをコピーして利用する
      * `--no-keep-backup` - 入力したバックアップをリストア時に削除する
      * `-f,--force` - 確認のプロンプトを表示させずに実行する
    * impl memo
      * if service is absent
        * (A)
          * exec service with `--restore-backup`
          * wait until service is absent
        * (B)
          * exec service with `--maintenance`
          * send `restore_backup` to "backup_service"
          * send `shutdown` to "control_service"
          * wait until service is absent
      * otherwise
        * raise error
  * `tag`
    * overview
      * Point-in-Time Recovery のタグ作成時点にデータベースをリストアする
    * options
      * `<tag-name>` - リストア対象の Point-in-Time recovery のタグ
      * `-f,--force` - 確認のプロンプトを表示させずに実行する
    * impl memo
      * if service is absent
        * (A)
          * exec service with `--restore-tag`
          * wait until service is absent
        * (B)
          * exec service with `--maintenance`
          * send `restore_tag` to "backup_service"
          * send `shutdown` to "control_service"
          * wait until service is absent
      * otherwise
        * raise error

### Point-in-Time Recovery

```sh
oltp tag list [--conf </path/to/conf>] [-v|--verbose]
oltp tag show <tag-name> [--conf </path/to/conf>]
oltp tag add <tag-name> [--comment <message>] [--conf </path/to/conf>]
oltp tag remove <tag-name> [-f|--force] [--conf </path/to/conf>]
```

* overview
  * Point-in-Time recovery のタグに関する操作を行う
* 共通
  * options
    * `--conf` - 設定ファイルのパス、未指定の場合は既定の設定パスを利用
  * note
    * 同一の設定ファイルを参照する、稼働中または未稼働のデータベースを対象にとる
    * 対象のデータベースが未稼働の場合、コマンドの終了まで一時的にデータベースをメンテナンスモードで起動する
      * メンテナンスモードでは、データベースプロセスを起動しているが利用可能な機能が大幅に制限されている
      * 作業完了後、データベースプロセスは完全に終了する
* サブコマンド
  * `list`
    * overview
      * 利用可能なタグ名の一覧を表示する
    * options
      * `-v,--verbose` - タグ名に加え、タグのコメント等の情報を併せて表示する
    * note
      * 本コマンドは、 `show` に比べて一部の情報が欠落している可能性がある
    * impl memo
      * if service is present
        * send `tag_list` to "backup_service"
        * print tag list
      * if service is absent
        * exec service with `--maintenance`
        * process as it was present
        * send `shutdown` to "control_service"
        * wait until service is absent
      * if service is unknown
        * raise error
  * `show`
    * overview
      * 対象のタグの情報を表示する
    * options
      * `<tag-name>` - 対象のタグ名
    * note
      * 本コマンドは、タグに含まれる、作成時刻やコメントなどの追加情報を表示する
      * 対象のタグが存在しない場合、このコマンドは失敗する
    * impl memo
      * if service is present
        * send `tag_info` to "backup_service"
        * print tag info
      * if service is absent
        * exec service with `--maintenance`
        * process as it was present
        * send `shutdown` to "control_service"
        * wait until service is absent
      * if service is unknown
        * raise error
  * `add`
    * overview
      * タグを新たに登録
    * options
      * `<tag-name>` - 対象のタグ名
      * `--comment` - タグコメント
    * note
      * 作成したタグは `oltp restore tag` で復元できる
      * 対象のタグ名が既に登録済みである場合、このコマンドは失敗する
      * 対象のデータベースが稼働中の場合、現在までの処理結果の反映を待ち合わせるため、多少時間がかかる場合がある
    * impl memo
      * if service is present
        * send `tag_add` to "backup_service"
      * if service is absent
        * exec service with `--maintenance`
        * process as it was present
        * send `shutdown` to "control_service"
        * wait until service is absent
      * if service is unknown
        * raise error
  * `remove`
    * overview
      * 対象のタグの登録を解除する
    * options
      * `<tag-name>` - 対象のタグ名
      * `-f,--force` - 確認プロンプトなしで処理を続行する
    * note
      * 対象のタグ名の登録が存在しない場合、このコマンドは何も行わない
      * `oltp tag list | grep '...' | xargs oltp tag remove -f` が通る
    * impl memo
      * if service is present
        * send `tag_remove` to "backup_service"
      * if service is absent
        * exec service with `--maintenance`
        * process as it was present
        * send `shutdown` to "control_service"
        * wait until service is absent
      * if service is unknown
        * raise error

### コマンドグループ

```sh
oltp [-h|--help]
oltp backup [-h|--help]
oltp restore [-h|--help]
oltp tag [-h|--help]
```

* overview
  * 各種コマンドグループのヘルプ情報を表示する
* options
  * `-h,--help` - ヘルプ情報を表示する

### ヘルプ情報

```sh
<any commands> [-h|--help]
```

* overview
  * コマンドを実行する代わりに、コマンドのヘルプ情報を表示する
* options
  * `-h,--help` - ヘルプ情報を表示する

### 認証 (案)

* 認証情報は共通オプションで指定する
  * `--credential </path/to/credential>`
    * 認証情報ファイルを指定する
  * オプション未指定の場合、コマンド起動後にプロンプトを表示
    * user/password を入力

## ユーザーコマンド案

* コマンド体系を分ける？
  * ユーザーコマンドと管理コマンドとでルートコマンド自体を分けてもよさそう？
  * `psql` v.s. `pgAdmin`

### SQL実行

```sh
oltp exec </path/to/statement-list> [-v|--verbose] <connection-options>
oltp shell [-e <sql-statement>] <connection-options>

connection-options:
  [--conf </path/to/conf>] |
  --url <tcp://host:port>
```

* overview
  * SQLコマンドを実行する
* 共通
  * options
    * `--conf` - 設定ファイルのパス、未指定の場合は既定の設定パスを利用
    * `--url` - 接続先サーバーのURL
  * note
    * `--conf`, `--url` がいずれも未指定の場合、 `--conf` で既定の設定パスが指定されたものとして扱う
    * `--conf` が指定された場合、 `--conf` に指定された設定ファイルを利用する、稼働中のデータベースに接続する
    * `--url` が指定された場合、対象の URL が指す稼働中のデータベースに接続する
    * 対象のデータベースが稼働中でない場合、このコマンドは失敗する
* サブコマンド
  * `exec`
    * overview
      * ファイルを指定して SQL 文のリストを順に実行する
    * options
      * `</path/to/statement-list>` - SQL 文のリスト
    * `-v,--verbose` - 詳細情報の表示
    * note
      * SQL の各文は、セミコロン文字 `;` で区切られている必要がある
    * impl memo
      * connect to service, or raise error
      * split statements
      * for each statement
        * prepare statement
        * if it is transaction option statement
          * collect the transaction option
          * end process about this statement
        * if it is the first statement
          * begin transaction with collected options
        * if statement has result set (TBD: require `ps.has_result_set`)
          * execute the statement as query
          * for each resulting row
            * print row
        * else if statement does not have result set
          * execute the statement
      * commit transaction if it was began
  * `shell`
    * overview
      * SQL 文を実行するシェルを起動する
    * options
      * `-e` - 指定された場合、引数の SQL 文字列を実行して即座に終了する
    * note
      * このコマンドを実行すると、プロンプトが表示されSQL文を繰り返し入力できる
      * SQL の各文は、終端にセミコロン文字 `;` を指定する必要がある
        * `-e` で指定した場合には不要
    * impl memo
      * process as like `exec`

### dump/load

```sh
oltp data dump <table-name> </path/to/dump-target> [--fresh|--safe] [--overwrite] [-v|--verbose]
oltp data load <table-name> </path/to/load-source>.. [--insert|--skip|--replace|--truncate] [--transaction|--no-transaction] [-f|--force] [-v|--verbose] <connection-options>

connection-options:
  --conf </path/to/conf> |
  --url <tcp://host:port>
```

* overview
  * テーブルのダンプ・ロードを行う
* 共通
  * options
    * `--conf` - 設定ファイルのパス、未指定の場合は既定の設定パスを利用
    * `--url` - 接続先サーバーのURL
    * `-v,--verbose` - 詳細情報の表示
  * note
    * `--conf`, `--url` がいずれも未指定の場合、 `--conf` で既定の設定パスが指定されたものとして扱う
    * `--conf` が指定された場合、 `--conf` に指定された設定ファイルを利用する、稼働中のデータベースに接続する
    * `--url` が指定された場合、対象の URL が指す稼働中のデータベースに接続する
    * 対象のデータベースが稼働中でない場合、このコマンドは失敗する
* サブコマンド
  * `dump`
    * overview
      * 指定したテーブルの内容を全て出力する
    * options
      * `<table-name>` - 対象のテーブル名
      * `</path/to/dump-target>` - ダンプファイル出力先のディレクトリ
      * `--fresh` - 最新のデータを読むが、アボートする場合がない
      * `--safe` (default) - 多少古いデータを読むが、アボートしにくい
      * `--overwrite` - 出力先にディレクトリが既に存在していた場合、削除してからダンプファイルを出力する
    * note
      * 出力先に空でないディレクトリが既に存在し、かつ `--overwrite` が指定されていない場合、コマンドは失敗する
      * このコマンドはデータベースが稼働しているコンピューターの、ローカルファイルシステム上にダンプファイルを出力する
    * impl memo
      * connect to service, or raise error
      * check the target directory
      * if `--safe`
        * begin transaction as read only mode
      * if `--fresh`
        * begin long transaction without any write preservation
      * execute dump
      * commit transaction
  * `load`
    * overview
      * SQL 文を実行するシェルを起動する
    * options
      * `<table-name>` - 対象のテーブル名
      * `</path/to/load-target>` - ロード対象のファイル
      * `--insert` (default) - 既に存在する行があれば、エラー終了する
      * `--skip` - 既に存在する行があれば、スキップする
      * `--replace` - 既に存在する行があれば、上書きする
      * `--truncate` - 全ての行を削除してからロードする
      * `--transaction` (default) - 単一のトランザクション内で実行する
      * `--no-transaction` - 複数のトランザクションに分割して実行する
      * `-f,--force` - プロンプトなしで実行する
    * note
      * 実行中にエラーが発生した場合、 `--transaction` が指定されていれば全ての処理を巻き戻し、 `--no-transaction` が指定されていれば一部の処理が適用済みになる
        * `--insert` などの冪等でない処理を行う際には十分な注意が必要
      * このコマンドはデータベースが稼働しているコンピューターの、ローカルファイルシステムからダンプファイルを読みだす
      * ロードするファイルは所定の形式でなければならず、CSV形式などは事前に変換しておく必要がある
    * impl memo
      * connect to service, or raise error
      * check the target files
      * extract column name and types from the target files
      * with `--insert`
        * prepare SQL starting with `"INSERT INTO ..."`
      * with `--skip`
        * prepare SQL starting with `"INSERT IF NOT EXISTS INTO ..."`
      * with `--replace` or `--truncate`
        * prepare SQL starting with `"INSERT OR UPDATE INTO ..."`
      * if `--no-transaction`, run the following sequence for the individual input files
      * begin transaction with write preservation to the target table
      * with `--truncate`, execute truncate the target table
      * execute load
      * commit transaction
    * impl TBD
      * dump file から column name/type の抽出を誰がやる？
