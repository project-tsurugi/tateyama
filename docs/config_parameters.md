# 構成ファイルのパラメーター

tsurugiの構成ファイルは下記のようなiniファイル形式でコンフィグパラメーターを設定する事ができる。

```
[section]
parameter=value
```

下記にセクションごとにサポートされるパラメーターの一覧を列挙する。

## data_store

セクション名: data_store

対象コンポーネント
- データストア
- トランザクショナルKVS(shirakami)

### log_location

パラメーター名: log_location
型: 文字列
値: ログを保存するディレクトリへの絶対パス

## sql 

セクション名: sql

対象コンポーネント
- SQLサービス・リソース(jogasaki)

### thread_pool_size

パラメーター名: thread_pool_size
型: 整数
値: SQLサービスが使用するタスクスケジューラの使用するスレッド数
備考: 開発用のため将来的に削除/変更される可能性あり

### lazy_worker

パラメーター名: lazy_worker
型: ブール(true/false)
値: SQLサービスが使用するタスクスケジューラが省CPUのためにワーカースレッドをスリープさせるかどうか
備考: 開発用のため将来的に削除される可能性あり
