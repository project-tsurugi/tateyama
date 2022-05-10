# Tateyama component 一覧

2022-04-16 arakawa (NT)

## この文書について

* tateyama を整理した結果必要になりそうなコンポーネント群
  * 細分化しているため、合成してもよい

## core features

tateyama を実行する上で必須となるであろう組み込みコンポーネント群

### core.resource

* `mutex_resource`
  * 占有リソースの排他機能を実現するリソース
  * depends on
    * N/A
* `status_resource`
  * サーバーのステータス情報を統括するリソース
  * depends on
    * N/A
* `session_resource`
  * セッション期間の情報を保持するリソース
  * depends on
    * N/A
* `task_scheduler_resource`
  * タスクにスレッドを割り当てる
  * depends on
    * N/A
* `transactional_kvs_resource`
  * Transactional KVS
  * depends on
    * N/A
  * TBD
    * naming
* `datastore_resource`
  * CCエンジンのログを保存するデータストア
  * depends on
    * `transactional_kvs_resource`

### core.service

* `routing_service`
  * endpoint -> service へのメッセージ配信を行う
  * depends on
    * `session_resource` ?
* `control_service`
  * サーバーを制御する
    * handle `status` command
    * handle `shutdown` command
    * handle `throttle` command ?
    * handle `keep_alive` command ?
      * セッションの lease 期限を延ばす
  * depends on
    * `status_resource` ?
* `datastore_service`
  * データストアの操作を提供する
    * handle `backup_begin` command
    * handle `backup_end` command
    * handle `backup_continue` command
    * handle `backup_estimate` command
    * handle `restore_backup` command (only if maintenance)
    * handle `restore_tag` command (only if maintenance)
    * handle `tag_list` command
    * handle `tag_get` command
    * handle `tag_add` command
    * handle `tag_remove` command
    * expose restore from backup function
    * expose restore from tag function
  * depends on
    * `datastore_resource`

### core.endpoint

* `ipc_endpoint`
  * IPC経由でクライアントとコマンドを授受する
  * depends on
    * `routing_service`
    * `session_resource` ?
      * `routing_service` を経由してもよい

## SQL features

### sql.resource

* `sql_resource`
  * SQL機能を提供するリソース
  * depends on
    * `task_scheduler_resource`
    * `transactional_kvs_resource`
    * `datastore_resource` ?
      * blob コンテンツなどはこちらから読む？
  * TBD
    * secondary index, table schema, constraints, trigger 等を意識した KVS interface を提供してくれると C++ native app を作りやすそう？
      * そういう意味ではもう一つレイヤを増やすべきか？

### sql.service

* `sql_service`
  * SQL機能をコマンド経由で利用するサービス
    * transaction control
      * handle `transaction_begin` command
      * handle `transaction_commit` command
      * handle `transaction_rollback` command
    * prepare statement
      * handle `statement_prepare` command
      * handle `statement_dispose` command
    * metadata
      * handle `describe_statement` command
      * handle `describe_table` command
    * SQL operations
      * handle `execute` command
      * handle `query` command
    * bulk data operations
      * handle `dump` command
      * handle `load` command
  * depends on
    * `sql_resource`
    * `session_resource`

## other features

### other.endpoint

* `tcp_endpoint`
  * TCP/IP 経由でクライアントとコマンドを授受する
  * depends on
    * `routing_service`
