# 構成ファイルのパラメーター

tsurugiの構成ファイルは下記のようなiniファイル形式でコンフィグパラメーターを設定する事ができる。

```
[section]
parameter=value
```

下記にセクションごとにサポートされるパラメーターの一覧を列挙する。

## datastoreセクション

セクション名
  - datastore

対象コンポーネント

  - データストア
  - トランザクショナルKVS(shirakami)

|                  パラメーター名 | 型 | 値                   | 備考                                                                                                                                                           |
|-------------------------:| :---: |:--------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------|
|             log_location | 文字列 | ログを保存するディレクトリへの絶対パス | デフォルトはshirakamiの決めるデフォルトパス([database_options::log_directory_path_の記述](https://github.com/project-tsurugi/shirakami/blob/master/docs/database_options.md)を参照) |
| logging_max_parallelism  | 整数 | 同時並行に動作するロガーの最大数    | 開発中のためまだ完全には動作しない。デフォルトは112                                                                                                                                  |

## sqlセクション

セクション名
  - sql

対象コンポーネント
  - SQLサービス・リソース(jogasaki)

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
|thread_pool_size | 整数 | SQLサービスが使用するタスクスケジューラの使用するスレッド数|開発用のため将来的に削除/変更される可能性あり|
|lazy_worker | ブール(true/false) | SQLサービスが使用するタスクスケジューラが省CPUのためにワーカースレッドをスリープさせるかどうか|開発用のため将来的に削除される可能性あり|
|prepare_qa_tables | ブール(true/false) | DB起動時にQA用のbuilt-inテーブル定義を追加するか。デフォルトはfalse|開発用のため将来的に削除される可能性あり|
|prepare_phone_bill_tables | ブール(true/false) | DB起動時に電話料金計算バッチ用のbuilt-inテーブル定義を追加するか。デフォルトはfalse|開発用のため将来的に削除される可能性あり|

## ipc_endpointセクション

セクション名
  - ipc_endpoint

対象コンポーネント
  - ipc_endpoint(tateyama)

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
|database_name | 文字列 | ipc_endpointに接続する際のurl名、default値はtateyama（参）この文字列は/dev/shmに作成されるファイル名のprefixとして使われる
|threads | 整数 | ipc_endpointの最大同時接続数

## stream_endpointセクション

セクション名
  - stream_endpoint

対象コンポーネント
  - stream_endpoint(tateyama)

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
|port | 整数 | stream_endpointに接続する際のport番号、default値は12345
|threads | 整数 | stream_endpointの最大同時接続数

## fdwセクション

セクション名
  - fdw

対象コンポーネント
  - ogawayama

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
|name | 整数 | 文字列 | ogawayamaと接続する際のurl名、default値はtateyama（参）この文字列は/dev/shmに作成されるファイル名のprefixとして使われる
|threads | 整数 | fdwの最大同時接続数

## sessionセクション

セクション名
  - session

対象コンポーネント
  - セッションを利用するtateyamaのコンポーネント全て

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
|enable_timeout | ブール(true/false) | セッションの自動タイムアウトの有効性、デフォルト値はtrue
|refresh_timeout | 整数 | 通信によるセッションのライフタイム延長時間(秒)、デフォルト値は300
|max_refresh_timeout | 整数 | 明示的な要求によるセッションのライフタイム延長時間の最大値(秒)、デフォルト値は10800
