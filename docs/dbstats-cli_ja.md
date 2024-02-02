# データベースメトリクス情報の提供

* 2023-12-19 arakawa (NT)
  * 初版
* 2023-12-20 arakawa (NT)
  * 複数の値を持つメトリクス項目を反映

## この文書について

* この文書では、 Tsurugi インスタンスのメトリクスを取得するためのコマンドについて詳細を示す。

## コンセプト

* Tsurugi の内部でしか測定できない情報を提供する
  * OSから取得できるような情報は対象外 (e.g. tsurugidbプロセス全体の使用メモリ)
* 10秒~1分程度の間隔でコマンドを利用することを想定
  * あまり短い間隔でのコマンドの連続利用を想定していない
* 提供される情報は主に概算値
  * 厳密な値を提供すると性能に影響があるため、基本は概算値 (e.g. テーブルの行数)
* 提供される情報は最新とは限らない
  * 定期的に測定を行い、そのタイミングで更新を行うので必ずしも最新を反映しない
* 提供される情報の間に一貫性はない
  * 個々の項目の測定は独立して行われるため、項目間の一貫性は保証されない

## CLI

### dbstats show

`tgctl dbstats show` - Tsurugi 内部のメトリクス情報を表示する

```sh
tgctl dbstats show
```

* mandatory parameters
  * なし
* optional parameters
  * `--conf </path/to/tsurugi.ini>`
    * Tsurugi構成定義ファイルのパス
    * 未指定の場合は `tgctl` の標準的な方法で当該ファイルを検出する
  * `--format <format-name>`
    * メトリクス情報の表示形式
    * 利用可能な項目
      * `text`
        * 人間が読むことを想定したテキスト形式
      * `json`
        * JSON形式 ([show サブコマンドの JSON 形式の出力](#show-サブコマンドの-json-形式の出力))
    * 未指定の場合は `text`
  * `--monitor </path/to/monitor.jsonl>`
    * 指定のパスにモニタリング情報を出力する
    * 未指定の場合はモニタリング情報を出力しない
  * TBD: 認証等は tgctl の標準的なオプションに従う
* standard output
  * `--format text`
    * 各メトリクス項目のキーとその値を出力する
  * `--format json`
    * [show サブコマンドの JSON 形式の出力](#show-サブコマンドの-json-形式の出力) で規定された JSON オブジェクトを出力する
* standard error
  * エラー発生時に、そのエラーの内容を表示する
* monitoring information
  * メトリクス情報表示時に、以下の  `kind=data` のレコード (`format=dbstats`) を出力する

    フィールド名 | 内容 | 備考
    ------|------|------
    `kind` | `data` |
    `format` | `dbstats` |
    `metrics` | メトリクス情報 | [show サブコマンドの JSON 形式の出力](#show-サブコマンドの-json-形式の出力)

* exit status

  reason | exit status | 概要
  -------|-------------|------
  (absent) | `0` | 正常にメトリクス情報を出力できた
  `invalid_parameter` | 非 `0` | いずれかのオプションが不正な値 (モニタリング情報は出力できない場合がある)
  `monitor_output` | 非 `0` | モニタリング情報の出力中にエラーが発生した (モニタリング情報は出力できない場合がある)
  `authentication_failure` | 非 `0` | データベース接続時に認証エラーが発生した
  `server_not_started` | 非 `0` | 対象のデータベースが稼動していない
  `connection_timeout` | 非 `0` | 対象のデータベースへの接続要求がタイムアウトした
  `connection_failure` | 非 `0` | 対象のデータベースへの接続に失敗した
  `io` | 非 `0` | ハンドルできないI/Oエラーが発生した
  `server` | 非 `0` | ハンドルできないサーバーエラーが発生した
  `unknown` | 非 `0` | ハンドルできない任意のエラーが発生した
  `interrupted` | 非 `0` | 割り込みが発生した
  `internal` | 非 `0` | アプリケーション側の問題による内部エラー

  ※ reason はモニタリング情報のコマンド終了時の原因コード、 exit status はコマンド自体の終了ステータス

* 備考
  * 将来的に、 `tgctl dbstats show "index.*"` のように、表示する内容のフィルタを掛けられるようにすることを想定

### dbstats list

`tgctl dbstats list` - 利用可能な Tsurugi のメトリクス項目一覧とその説明を表示する。

```sh
tgctl dbstats list
```

* mandatory parameters
  * なし
* optional parameters
  * `--conf </path/to/tsurugi.ini>`
    * Tsurugi構成定義ファイルのパス
    * 未指定の場合は `tgctl` の標準的な方法で当該ファイルを検出する
  * `--format <format-name>`
    * メトリクス項目一覧の表示形式
    * 利用可能な項目
      * `text`
        * 人間が読むことを想定したテキスト形式
      * `json`
        * JSON形式 ([list サブコマンドの JSON 形式の出力](#list-サブコマンドの-json-形式の出力))
    * 未指定の場合は `text`
  * `--monitor </path/to/monitor.jsonl>`
    * 指定のパスにモニタリング情報を出力する
    * 未指定の場合はモニタリング情報を出力しない
  * TBD: 認証等は tgctl の標準的なオプションに従う
* standard output
  * `--format text`
    * 各メトリクス項目のキーとその項目の説明を出力する
  * `--format json`
    * [list サブコマンドの JSON 形式の出力](#list-サブコマンドの-json-形式の出力) で規定された JSON オブジェクトを出力する
* standard error
  * エラー発生時に、そのエラーの内容を表示する
* monitoring information
  * メトリクス情報表示時に、以下の  `kind=data` のレコード (`format=dbstats-description`) を出力する

    フィールド名 | 内容 | 備考
    ------|------|------
    `kind` | `data` |
    `format` | `dbstats-description` |
    `metrics` | メトリクス項目一覧 | [list サブコマンドの JSON 形式の出力](#list-サブコマンドの-json-形式の出力)

* exit status

  reason | exit status | 概要
  -------|-------------|------
  (absent) | `0` | 正常にメトリクス項目一覧の説明を出力できた
  `invalid_parameter` | 非 `0` | いずれかのオプションが不正な値 (モニタリング情報は出力できない場合がある)
  `monitor_output` | 非 `0` | モニタリング情報の出力中にエラーが発生した (モニタリング情報は出力できない場合がある)
  `authentication_failure` | 非 `0` | データベース接続時に認証エラーが発生した
  `server_not_started` | 非 `0` | 対象のデータベースが稼動していない
  `connection_timeout` | 非 `0` | 対象のデータベースへの接続要求がタイムアウトした
  `connection_failure` | 非 `0` | 対象のデータベースへの接続に失敗した
  `io` | 非 `0` | ハンドルできないI/Oエラーが発生した
  `server` | 非 `0` | ハンドルできないサーバーエラーが発生した
  `unknown` | 非 `0` | ハンドルできない任意のエラーが発生した
  `interrupted` | 非 `0` | 割り込みが発生した
  `internal` | 非 `0` | アプリケーション側の問題による内部エラー

  ※ reason はモニタリング情報のコマンド終了時の原因コード、 exit status はコマンド自体の終了ステータス

* 備考
  * 将来的に、 `tgctl dbstats list "index.*"` のように、表示する内容のフィルタを掛けられるようにすることを想定

## メトリクス項目

利用可能なメトリクス項目は以下のとおりである (未実装の場合、当該項目は出力されない)。

キー名 | 説明 | 値の型 | 備考
------|------|--------|-----
`session_count` | "number of active sessions" | int |
`table_count` | "number of user tables" | int | `index_size` の集約
`index_size` | "estimated each index size" | array of [`index_size`](#index_size) |
`storage_log_size` | "transaction log disk usage" | int | バイト単位
`storage_snapshot_size` | "snapshot disk usage" | int | バイト単位
`ipc_buffer_size` | "allocated buffer size for all IPC sessions" | int | バイト単位
`sql_buffer_size` | "allocated buffer size for SQL execution engine" | int | バイト単位

なお、「キー名」は [JSON 形式の出力](#json-形式の出力) におけるプロパティ名としても利用する。また、「説明」は [`tgctl dbstats list`](#dbstats-list) で表示する。

以下は追加を検討しているメトリクス項目の一覧である。

キー名 | 説明 | 値の型 | 備考
------|------|--------|-----
`table_size` | "estimated each table size" | array of [`table_size`](#table_size) | `index_size` の集約
`index_count` | "number of user indices" | int | `index_size` の集約
`index_total_size` | "estimated total index size" | int | バイト単位, `index_size` の集約
`table_index_count` | "number of indices in each table" | array of [`table_index_count`](#table_index_count) | `index_size` の集約
`table_row_count` | "number of estimated rows in each table" | array of [`table_row_count`](#table_row_count) |
`table_key_size` | "average of estimated key size of each table" | array of [`table_key_size`](#table_key_size) | `index_key_size` からの抜粋
`table_value_size` | "average of estimated value size of each table" | array of [`table_value_size`](#table_value_size) | `index_value_size` からの抜粋
`table_total_size` | "estimated total table size" | int | バイト単位, `index_size` の集約
`index_key_size` | "average of estimated key size of each index" | array of [`index_key_size`](#index_key_size) |
`index_value_size` | "average of estimated value size of each index" | array of [`index_value_size`](#index_value_size) |

### table_size

プロパティ名 | 値の型 | 概要
------------|--------|-----
`table_name` | string | 対象のテーブル名
`value` | int | テーブルサイズ (バイト単位, 関連インデックスを含む)

### table_index_count

プロパティ名 | 値の型 | 概要
------------|--------|-----
`table_name` | string | 対象のテーブル名
`value` | int | 総インデックス数 (プライマリインデックスを含む)

### table_row_count

プロパティ名 | 値の型 | 概要
------------|--------|-----
`table_name` | string | 対象のテーブル名
`value` | int | 行数

### table_key_size

プロパティ名 | 値の型 | 概要
------------|--------|-----
`table_name` | string | 対象のテーブル名
`value` | double | 平均キー長 (バイト単位)

### table_value_size

プロパティ名 | 値の型 | 概要
------------|--------|-----
`table_name` | string | 対象のテーブル名
`value` | double | 平均値長 (バイト単位)

### index_size

プロパティ名 | 値の型 | 概要
------------|--------|-----
`index_name` | string | 対象のインデックス名
`table_name` | string | 対象のインデックスが属するテーブル名
`value` | int | インデックスサイズ (バイト単位)

### index_key_size

プロパティ名 | 値の型 | 概要
------------|--------|-----
`index_name` | string | 対象のインデックス名
`table_name` | string | 対象のインデックスが属するテーブル名
`value` | double | 平均キー長 (バイト単位)

### index_value_size

プロパティ名 | 値の型 | 概要
------------|--------|-----
`index_name` | string | 対象のインデックス名
`table_name` | string | 対象のインデックスが属するテーブル名
`value` | double | 平均値長 (バイト単位)

## JSON 形式の出力

### show サブコマンドの JSON 形式の出力

メトリクス情報を JSON 形式で出力する場合、JSON オブジェクトが一つだけ出力される。
このオブジェクトの各プロパティは単一のメトリクス項目を表しており、プロパティ名にメトリクス項目のキー名、プロパティ値にメトリクス項目の値をとる。

プロパティ値は整数、浮動小数点数、配列のいずれかであり、配列の場合は各要素が以下のプロパティを有するオブジェクトである。

プロパティ名 | 値の型 | 概要 | 備考
------------|--------|-----|-------
(任意) | string | 要素のタイトル | 1件以上必要
`value` | int/double | 要素の値 | 配列のネストは不可

以下は、JSON形式による出力の例である。

```json
{
  "session_count": 100,
  "table_count": 3,
  "index_size":  [
    {
        "table_name": "A",
        "index_name": "IA1",
        "value": 65536,
    },
    {
        "table_name": "A",
        "index_name": "IA2",
        "value": 16777216,
    },
    {
        "table_name": "B",
        "index_name": "IB1",
        "value": 256,
    }
  ],
  "storage_log_size": 65535,
  "ipc_buffer_size": 1024,
  "sql_buffer_size": 268435456
}
```

### list サブコマンドの JSON 形式の出力

メトリクス項目の一覧を JSON 形式で出力する場合、JSON オブジェクトが一つだけ出力される。
このオブジェクトの各プロパティは単一のメトリクス項目を表しており、プロパティ名にメトリクス項目のキー名、プロパティ値にその項目の説明をとる。

以下は、JSON形式による出力の例である。

```json
{
  "session_count": "number of active sessions",
  "table_count": "number of user tables",
  "index_size": "estimated each index size",
  "storage_log_size": "transaction log disk usage",
  "ipc_buffer_size": "allocated buffer size for all IPC sessions",
  "sql_buffer_size": "allocated buffer size for SQL execution engine"
}
```
