# Altimeter loggerをコントロールするCLI

* 2023-10-17 horikawa (NT)
  * 初版

## この文書について

* Altimeter logger の設定や動作をDBAなどが外部から制御するCLIの仕様を示す

## 基本的なデザイン

* Altimeter logger の下記設定や動作を制御するCLIを提供する
  * 有効化｜無効化する
  * log levelの変更
  * log対象とするステートメント実行時間閾値の変更
  * log rotateを起動する

## CLI

### 有効化、無効化

Altimeter loggerを有効化または無効化する。対象は、event logまたはaudit log

```sh
tgctl altimeter {enable|disable} {event|audit} [--conf </path/to/conf>] [--monitor </path/to/log.jsonl>]
```

* parameter
  * {enable|disable} logに対する操作を指定する。enableは有効化、disableは無効化
  * {event|audit} 操作対象logを指定する。eventはイベントログ、auditは監査ログ
* options
  * `--conf` - 設定ファイルのパス、未指定の場合は既定の設定パスを利用
  * `--monitor` - 指定されたファイルにモニタリング情報を出力する
* exit status
  * 操作を正しく実施できた場合、 `0`
  * 操作を正しく実施できなかった場合、非 `0`
* standard error
  * 非 `0` の exit status を返す場合、エラーの内容を表示
* monitor
  * 固有の出力は存在しない
  * reason code
    * 操作を正しく実施できた場合：（absent）
    * 操作を正しく実施できなかった場合
      * 対象のデータベースに接続できなかった場合:`connection`
      * サーバ側で処理中にエラーとなった場合：`server`
* example

  ```sh
  $ tgctl altimeter enable event
  ```

### log level変更

Altimeter loggerのlog levelを変更する。対象は、event logまたはaudit log

```sh
tgctl altimeter set {event_level|audit_level} <level-value> [--conf </path/to/conf>] [--monitor </path/to/log.jsonl>]
```

* parameter
  * {event_level|audit_level} 変更対象のlog levelを指定する。event_levelはevent logのlevel、audit_levelはaudit logのlevel
  * <level-value>　変更するlog level（数値）
* options
  * `--conf` - 設定ファイルのパス、未指定の場合は既定の設定パスを利用
  * `--monitor` - 指定されたファイルにモニタリング情報を出力する
* exit status
  * 操作を正しく実施できた場合、 `0`
  * 操作を正しく実施できなかった場合、非 `0`
* standard error
  * 非 `0` の exit status を返す場合、エラーの内容を表示
* monitor
  * 固有の出力は存在しない
  * reason code
    * 操作を正しく実施できた場合：（absent）
    * 操作を正しく実施できなかった場合
      * 対象のデータベースに接続できなかった場合:`connection`
      * サーバ側で処理中にエラーとなった場合：`server`
* example

  ```sh
  $ tgctl altimeter set event_level 10
  ```

### log対象とするステートメント実行時間閾値の変更

Altimeterのevent logにおけるlog対象ステートメント実行時間閾値を変更する。

```sh
tgctl altimeter set statement_duration <duration-nanos> [--conf </path/to/conf>] [--monitor </path/to/log.jsonl>]
```

* parameter
  * <duration-nanos> 変更するlog対象ステートメント実行時間閾値、単位はナノ秒
* options
  * `--conf` - 設定ファイルのパス、未指定の場合は既定の設定パスを利用
  * `--monitor` - 指定されたファイルにモニタリング情報を出力する
* exit status
  * 操作を正しく実施できた場合、 `0`
  * 操作を正しく実施できなかった場合、非 `0`
* standard error
  * 非 `0` の exit status を返す場合、エラーの内容を表示
* monitor
  * 固有の出力は存在しない
  * reason code
    * 操作を正しく実施できた場合：（absent）
    * 操作を正しく実施できなかった場合
      * 対象のデータベースに接続できなかった場合:`connection`
      * サーバ側で処理中にエラーとなった場合：`server`
* example

  ```sh
  $ tgctl altimeter set statement_duration 1000000000
  ```

### log rotate起動

Altimeter loggerのrotate操作を起動する。対象は、event logまたはaudit log

```sh
tgctl altimeter rotate {event|audit} [--conf </path/to/conf>] [--monitor </path/to/log.jsonl>]
```

* parameter
  * {event|audit} 操作対象logを指定する。eventはイベントログ、auditは監査ログ
* options
  * `--conf` - 設定ファイルのパス、未指定の場合は既定の設定パスを利用
  * `--monitor` - 指定されたファイルにモニタリング情報を出力する
* exit status
  * 操作を正しく実施できた場合、 `0`
  * 操作を正しく実施できなかった場合、非 `0`
* standard error
  * 非 `0` の exit status を返す場合、エラーの内容を表示
* monitor
  * 固有の出力は存在しない
  * reason code
    * 操作を正しく実施できた場合：（absent）
    * 操作を正しく実施できなかった場合
      * 対象のデータベースに接続できなかった場合:`connection`
      * サーバ側で処理中にエラーとなった場合：`server`
* example

  ```sh
  $ tgctl altimeter rotate event
  ```