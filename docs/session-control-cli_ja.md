# セッションをコントロールするCLI

* 2022-11-01 arakawa (NT)
  * 初版
* 2023-11-30 arakawa (NT)
  * セッションスイッチに関するサブコマンドを追加
* 2024-02-02 arakawa (NT)
  * セッションスイッチをセッション変数に変更

## この文書について

* セッションをDBAなどが外部から制御するためのCLIについて、おおよその仕様を示す

## 基本的なデザイン

* セッションを制御するCLIを提供する
  * 新たに開始することはできず、セッションの表示、停止のみ
  * DBAおよびテスト・デバッグ向け用途
  * 上記用途のため、Java API は不要

## CLI

### session list

稼働中のセッション一覧を表示する。

```sh
tgctl session list [--conf </path/to/conf>] [--id] [-v|--verbose] [--monitor </path/to/log.jsonl>]
```

* options
  * `--conf` - 設定ファイルのパス、未指定の場合は既定の設定パスを利用
  * `--id` - session label の代わりに常に session ID を出力する
  * `-v,--verbose` - 全ての情報を表示する (`--id` の指定は無視される)
  * `--monitor` - 指定されたファイルにモニタリング情報を出力する
* exit status
  * 対象のセッション一覧を正しく取得できた場合、 `0` (`reason`: (absent))
  * 対象のデータベースに接続できなかった場合、非 `0` (`reason`: `connection`)
  * サーバ側で処理中にエラーとなった場合、非 `0` (`reason`: `server`)
* standard output
  * `--verbose` の指定がない場合
    * 稼働中のセッション一覧の session label を空白文字区切りで出力する
    * ただし、以下のいずれかの場合には session ID を表示する
      * session label が未定義または取得できなかった場合
      * 複数のセッションで session label が重複している場合
      * session label に区切り文字である空白文字が含まれる場合
  * `--verbose` の指定がある場合
    * 各セッションの以下の内容をテーブル形式で出力する

      列 | 内容 | 備考
      ------|------|------
      id | session ID | 先頭に `:` が付与されている
      label | session label | 不明の場合は空文字列
      application | アプリケーション名 | 不明の場合は空文字列
      user | ログインユーザ名 | 不明の場合は空文字列
      start | セッション開始時刻 | ISO 8601 形式のローカルタイム
      type | 接続種類 | ipc, tcp などの endpoint URI スキーマ名
      remote | 接続元情報 | IPC:PID, TCP:remote host/port

* standard error
  * 非 `0` の exit status を返す場合、エラーの内容を表示
* monitor
  * 以下の `kind=data` を稼働中のセッション数だけ繰り返す

    フィールド名 | 内容 | 備考
    ------|------|------
    `kind` | `data` |
    `format` | `session-info` |
    `session_id` | session ID | 先頭に `:` が付与されている
    `label` | session label | 不明の場合は空文字列
    `application`` | アプリケーション名 | 不明の場合は空文字列
    `user` | ログインユーザ名 | 不明の場合は空文字列
    `start_at` | セッション開始時刻 | Unix エポックからの経過ミリ秒
    `connection_type` | 接続種類 | ipc, tcp などの endpoint URI スキーマ名
    `connection_info` | 接続元情報 | IPC:PID, TCP:remote host/port

* example

  ```sh
  $ tgctl session list
  belayer-dump-1 example-1 example-2 :4
  ```

  ```sh
  $ tgctl session list --verbose
  id    label           application  user      start                 type  remote
  :1    belayer-dump-1  belayer      arakawa   2022-06-20T12:34:56Z  ipc   6502
  :2    example-1       tgsql        arakawa   2022-06-20T12:34:50Z  tcp   192.168.1.23:10000
  :3    example-2       tgsql        kurosawa  2022-06-20T12:34:53Z  tcp   192.168.1.78:10000
  :4                                 admin     2022-06-20T12:34:57Z  ipc   32816
  ```

* 備考
  * ID はとびとびになる
  * ID は必ずしも tateyama 内部の session ID と一致しない
  * ID は `:` から始まり、 label は `:` から始められない
  * verbose表示におけるstart時刻はUTCをISO 8601フォーマットで表示する
  * `tgctl session list` コマンドを実行したセッションは表示に含まれない

### session show

特定のセッションの情報を表示する。

```sh
tgctl session show <session-ref> [--conf </path/to/conf>] [-v|--verbose] [--monitor </path/to/log.jsonl>]
```

* options
  * `<session-ref>` - 対象の session ID または session label
  * `--conf` - 設定ファイルのパス、未指定の場合は既定の設定パスを利用
  * `-v,--verbose` - 無視される
  * `--monitor` - 指定されたファイルにモニタリング情報を出力する
* exit status
  * 対象のセッションの情報を正しく取得できた場合、 `0` (`reason`: (absent))
  * 対象のセッションが発見できない場合、 非 `0` (`reason`: `not_found`)
  * `<session-ref>` にラベルを指定し、かつラベルが一意でない場合、非 `0` (`reason`: `ambiguous`)
  * 対象のセッションに対し、当該操作が許容されなかった場合、非 `0` (`reason`: `permission`)
  * 対象のデータベースに接続できなかった場合、非 `0` (`reason`: `connection`)
  * サーバ側で処理中にエラーとなった場合、非 `0` (`reason`: `server`)
* standard output
  * 当該セッションの以下の情報を、各行に表示する

    ラベル | 内容 | 備考
    ------|------|------
    id | session ID | 先頭に `:` が付与されている
    application | アプリケーション名 | 不明の場合は空文字列
    label | session label | 不明の場合は空文字列
    user | ログインユーザ名 | 不明の場合は空文字列
    start | セッション開始時刻 | ISO 8601 形式のローカルタイム
    type | 接続種類 | ipc, tcp などの endpoint URI スキーマ名
    remote | 接続元情報 | IPC:PID, TCP:remote host/port

* standard error
  * 非 `0` の exit status を返す場合、エラーの内容を表示
* monitor
  * [session list](#session-list) と同様の出力を1件分だけ出力
* example

  ```sh
  $ tgctl session show load-1
  id           :cafebabe
  application  tgload
  label        load-1
  user         kambayashi
  start        2022-06-10-T01:23:45
  type         tcp
  remote       192.168.1.24:32768
  ```

### session shutdown

特定のセッションを強制的に終了させる。

```sh
tgctl session shutdown <session-ref> [<session-ref> [...]] [--conf </path/to/conf>] [-v|--verbose] [--forceful|--graceful] [--monitor </path/to/log.jsonl>]
```

* options
  * `<session-ref>` - 対象の session ID または session label
  * `--conf` - 設定ファイルのパス、未指定の場合は既定の設定パスを利用
  * `-v,--verbose` - 無視される
  * `--forceful` - forceful shutdownを行う
  * `--graceful` - graceful shutdownを行う
  * `-v,--verbose` - 無視される
  * `--monitor` - 指定されたファイルにモニタリング情報を出力する
* exit status
  * 対象のセッションに、終了要求を出せた場合 `0` (`reason`: (absent))
  * 対象のセッションが発見できない場合、 `0` (TBD) (`reason`: `not_found`)
  * `<session-ref>` にラベルを指定し、かつラベルが一意でない場合、非 `0` (`reason`: `ambiguous`)
  * 対象のセッションに対し、当該操作が許容されなかった場合、非 `0` (`reason`: `permission`)
  * 対象のデータベースに接続できなかった場合、非 `0` (`reason`: `connection`)
  * サーバ側で処理中にエラーとなった場合、非 `0` (`reason`: `server`)
* standard output
  * 何も表示しない
* standard error
  * 非 `0` の exit status を返す場合、エラーの内容を表示
* monitor
  * 固有の出力は存在しない
* example

  ```sh
  $ tgctl session shutdown example-1 example-3
  ```

* 備考
  * `xargs` と組み合わせて利用することを想定

    ```sh
    $ tgctl session list | xargs -L 1 tgctl session shutdown
    ```

  * `--forceful`と`--graceful`のどちらも指定されていない場合は、forceful shutdownを行う。両方とも指定された場合はエラー。

### session set

特定のセッション変数の値を変更する。

```sh
tgctl session set <session-ref> <variable-name> <variable-value>　[--conf </path/to/conf>] [-v|--verbose] [--monitor </path/to/log.jsonl>]
```

* options
  * `<session-ref>` - 対象の session ID または session label
  * `<variable-name>` - 対象のセッション変数の名称
  * `<variable-vaule>` - 対象のセッション変数に設定する値
  * `--conf` - 設定ファイルのパス、未指定の場合は既定の設定パスを利用
  * `-v,--verbose` - 無視される
  * `--monitor` - 指定されたファイルにモニタリング情報を出力する
* exit status
  * 対象のセッション変数を変更できた場合、 `0` (`reason`: (absent))
  * 対象のセッションが存在しない場合、 非 `0` (`reason`: `not_found`)
  * 対象のセッション変数が存在しない・または変更できない場合、非 `0` (`reason`: `permission`)
  * `<session-ref>` にラベルを指定し、かつラベルが一意でない場合、非 `0` (`reason`: `ambiguous`)
  * 対象のデータベースに接続できなかった場合、非 `0` (`reason`: `connection`)
  * 対象の変数が未定義の場合、 非 `0` (`reason`: `variable_not_defined`)
  * 対象の変数に対し値が不適切の場合、非 `0` (`reason`: `variable_invalid_value`)
  * サーバ側で処理中にエラーとなった場合、非 `0` (`reason`: `server`)
* standard output
  * 何も表示しない
* standard error
  * 非 `0` の exit status を返す場合、エラーの内容を表示
* monitor
  * 固有の出力は存在しない
* example

  ```sh
  # enable SQL execution plan recording in session "cost-batch-01"
  tgctl session set cost-batch-01 sql.plan-recording true
  ```

* 備考
  * プロファイリングツールで、特定のセッションだけ常に実行計画を記録したい、というケースで利用予定
  * `<session-ref>` には label または ID を指定する
  * 引数の label が一意でない場合、エラーになる
  * セッション変数の変更は他のサービス (e.g. SQL) から参照できる
  * セッション変数の一覧を [session show](#session-show) サブコマンドで表示してもよい
  * セッション変数名は case insensitive
  * `xargs` と組み合わせて利用するなら、 `<session-ref>` は末尾にあったほうが都合はいい
    * さらに言えば繰り返しができるとよい
    * ただし、微妙に分かりにくいCLIになるので避けた
