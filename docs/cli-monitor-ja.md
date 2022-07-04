# CLI の実行状況モニタリングオプション

## 目的

* 外部ツールが oltp コマンドを実行する際、進捗状況やエラー情報などを適切に検出できるようにする

## アプローチ

* 外部ツールが利用する CLI サブコマンドに、 `--monitor /path/to/output.json` オプションを追加
  * `--monitor` は非公開オプション
  * 出力先はローカルファイルを想定するが、NASなどに保存してもよいかもしれない
  * 上記ファイルには外部ツールに通知されるべき情報が、machine readable な形式で出力される
* 対象は以下のコマンド
  * [起動・終了](cli-spec-ja.md#%E8%B5%B7%E5%8B%95%E7%B5%82%E4%BA%86)
    * `oltp start`
    * `oltp shutdown`
    * `oltp kill`
    * `oltp quiesce`
    * `oltp status`
  * [backupサブコマンド](cli-spec-ja.md#backup-%E3%82%B5%E3%83%96%E3%82%B3%E3%83%9E%E3%83%B3%E3%83%89)
    * `oltp backup create`
  * [restoreサブコマンド](cli-spec-ja.md#restore-%E3%82%B5%E3%83%96%E3%82%B3%E3%83%9E%E3%83%B3%E3%83%89)
    * `oltp restore backup`
    * `oltp restore tag`
  * 上記は必須で、他にも必要なら対応

## フォーマット

### ファイルフォーマット

* 行ごとにちょうど1つのログレコードを持つテキストファイル
* 各レコードは JSON オブジェクトで表され、JSONオブジェクトに続いて改行文字 LF または CR+LF が来る
* 各JSONオブジェクトの内部に改行文字は含まれない
* ファイルは BOM なし UTF-8 エンコード

### レコード

* 各レコードは単一の JSON オブジェクトからなる
* レコードの種類によって JSON オブジェクトが持つフィールドの種類が異なる
* レコードの種類は以下
  * `start` - コマンド実行の開始
  * `finish` - コマンド実行の終了
  * `progress` - 進捗情報
  * `log` - 任意のメッセージ
* 全てのレコードに共通して、以下のフィールドを有する
  * `kind` - レコードの種類
  * `timestamp` - 発生時刻
* ファイルの先頭のレコードは `start` でなければならず、また `finish` 以降にレコードは出現しないことが望ましい
* 各コマンドは、レコードの出力ごとにファイルにフラッシュする
  * 外部ツールは `tail -f` のような仕組みで出力先ファイルを監視する想定

## 個々のレコードの内容

* `timestamp` フィールドは、 UNIX timestamp (秒)
* `timestamp` フィールドは共通なので以降の説明では省略する
* `code` はメッセージを他言語化するための仕組みで、 `arguments` はその引数
  * `code` が指定される場合、かならず `message` も指定される
  * `code` に対する i18n リソースが存在しない場合、 `message` を利用する (英語)

### `start`

| フィールド名 | 内容 |
|:-:|:--|
| `kind` | `start` |

### `finish`

| フィールド名 | 内容 |
|:-:|:--|
| `kind` | `finish` |
| `status` | `success`, `failure` |
| `message` | メッセージ (任意) |
| `code` | コード (任意) |
| `arguments` | コードに対する引数列 (配列型, 任意) |

### `progress`

| フィールド名 | 内容 |
|:-:|:--|
| `kind` | `progress` |
| `progress` | `[0.0,1.0]` の進捗率 |
| `message` | タスク名 (任意) |
| `code` | コード (任意) |
| `arguments` | コードに対する引数列 (配列型, 任意) |

### `log`

| フィールド名 | 内容 |
|:-:|:--|
| `kind` | `log` |
| `level` | `info`, `warn`, `error` のいずれか |
| `message` | ログメッセージ |
| `code` | コード (任意) |
| `arguments` | コードに対する引数列 (配列型, 任意) |

* 備考
  * fatal error の場合、 `log` ではなく `finish` の `status=failure` で通知する方が都合はいいが、必須ではない
  * 基本的に `info` はあまり利用せず、 `progress` レコードを利用する
