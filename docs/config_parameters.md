# 構成ファイルのパラメーター

tsurugiの構成ファイルは下記のようなiniファイル形式でコンフィグパラメーターを設定する事ができる。

```
[section]
parameter=value
```

下記にセクションごとにサポートされるパラメーターの一覧を列挙する。

## ccセクション
セクション名
  - cc

対象コンポーネント

  - トランザクショナルKVS(shirakami)

|                  パラメーター名 | 型 | 値                   | 備考                                                                                                                                                           |
|-------------------------:| :---: |:--------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------|
| epoch_duration  | 整数 | エポックの長さ(us)。デフォルトは3000。 |
| waiting_resolver_threads | 整数 | コミット待ちとなったLTX群の待ち判定とプレコミット処理を行うスレッド数。デフォルト2。 |

## datastoreセクション

セクション名
  - datastore

対象コンポーネント

  - データストア
  - トランザクショナルKVS(shirakami)

|                  パラメーター名 | 型 | 値                   | 備考                                                                                                                                                           |
|-------------------------:| :---: |:--------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------|
|             log_location | 文字列 | ログを保存するディレクトリへのパス | 必須パラメーターであり、設定がない場合はDBが起動に失敗する |
|  recover_max_parallelism | 整数 | リカバリ時の同時並行作業数。デフォルトは 8 | 開発用のため将来的に削除/変更される可能性あり |

## sqlセクション

セクション名
  - sql

対象コンポーネント
  - SQLサービス・リソース(jogasaki)

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
|thread_pool_size | 整数 | SQLサービスが使用するタスクスケジューラの使用するスレッド数。デフォルトは次式により環境に合わせて設定される。MIN( <既定ワーカー係数> * <物理コア数>, <最大既定ワーカー数> ) 結果が1未満になる場合は1とする。ここで、既定ワーカー係数 = 0.8、最大既定ワーカー数 = 32||
|enable_index_join | ブール(true/false) | 性能向上のためインデックスを利用した結合処理を行うか。デフォルトはtrue|開発用のため将来的に削除される可能性あり|
|stealing_enabled | ブール(true/false) | 空いたCPUコアを活用するためにスケジューラーがタスクのstealingを行うか。デフォルトはtrue||
|default_partitions | 整数 | 並列化可能な関係演算子の実行においてデータ分割を行う際のパーティション数。デフォルトは5||
|use_preferred_worker_for_current_thread | ブール(true/false) | スケジューラーがワーカーを選択する際、タスクを提出したスレッドごとに固定的なワーカーを使うようにするか。デフォルトはtrue|開発用のため将来的に削除される可能性あり|
|stealing_wait| 整数 | タスクスケジューラのワーカーがスティーリングを行う前に自身のタスクキューを余分にチェックする回数の係数。`<ワーカー数> * <stealing_wait>` 回だけ自身のタスクキューを多くチェックするようになる。デフォルトは1|開発用のため将来的に削除/変更される可能性あり|
|task_polling_wait| 整数 | タスクスケジューラのワーカーが自身のタスクキューやスティール候補のポーリングに失敗した際、次のポーリングまでスリープする時間(us)。デフォルトは0|開発用のため将来的に削除/変更される可能性あり|
|enable_hybrid_scheduler| ブール(true/false) | ハイブリッドスケジューラを使用するか。デフォルトはtrue|開発用のため将来的に削除される可能性あり|
|lightweight_job_level| 整数 | 短時間で終わるジョブを定義する閾値。設定値以下のlevelを持つジョブが短時間で終わるジョブとして分類され、リクエストスレッドで実行される候補となる。0を指定すると全てのジョブは短時間でないものとして扱われる。デフォルトは0。enable_hybrid_scheduler=trueのときのみ有効。ジョブのlevelは実行されるステートメントによって決まる。詳細は [`jogasaki::plan::statement_work_level_kind`](https://github.com/project-tsurugi/jogasaki/blob/497de010a018149cb22edc4ce780ab12734bc409/src/jogasaki/plan/statement_work_level.h#L24)を参照。 |開発用のため将来的に削除/変更される可能性あり|
|busy_worker| ブール(true/false) | タスクスケジューラのワーカーが高頻度でタスクキューをチェックするか。デフォルトはfalse|開発用のため将来的に削除される可能性あり|
|watcher_interval| 整数 | タスクスケジューラの条件確認スレッドが一時停止してから次に再開するまでの間隔(us)。デフォルトは1000|開発用のため将来的に削除/変更される可能性あり|
|worker_try_count| 整数 | タスクスケジューラのワーカーがサスペンド前にタスクキューを確認する回数。busy_worker=falseの場合のみ有効。デフォルトは1000|開発用のため将来的に削除/変更される可能性あり|
|worker_suspend_timeout| 整数 | タスクスケジューラのワーカーがサスペンドしてから復帰するまでの時間(us)。busy_worker=falseの場合のみ有効。デフォルトは1000000|開発用のため将来的に削除/変更される可能性あり|
|commit_response| 文字列 | commit待ち合わせの既定値。次のいずれかから選択 (ACCEPTED, AVAILABLE, STORED, PROPAGATED)。デフォルトはSTORED|クライアントからコミット時に明示的に指定することで、トランザクションごとに上記設定を上書き可能|
|dev_update_skips_deletion| ブール(true/false) | UPDATE文の更新対象列が主キーや索引列でない場合はレコードの削除を可能な限りスキップする。デフォルトはfalse|開発用のため将来的に削除される可能性あり|
|dev_profile_commits| ブール(true/false) | コミット処理の性能計測のためのプロファイリング情報を出力するか。デフォルトはfalse|開発用のため将来的に削除される可能性あり|
|dev_return_os_pages| ブール(true/false) | メモリ管理用のページプールに返却されたページをOSに返却するか。デフォルトはfalse|開発用のため将来的に削除される可能性あり|
|dev_omit_task_when_idle| ブール(true/false) | durable待ちトランザクションがない場合にdurability callback処理を行うタスクのスケジュールをスキップするか。デフォルトはtrue|開発用のため将来的に削除される可能性あり|
|plan_recording| ブール(true/false) | Altimeterのイベントログに `stmt_explain` 項目(SQL実行計画)を出力するか。デフォルトはfalse||
|dev_try_insert_on_upserting_secondary| ブール(true/false) | INSERT OR REPLACEをセカンダリインデックスのある表に対して実行する際、INSERTを事前に実行する最適化を実施するか。デフォルトはtrue|開発用のため将来的に削除される可能性あり|
|dev_scan_concurrent_operation_as_not_found| ブール(true/false) | スキャン操作において並列して挿入されたレコードを検知した際(WARN_CONCURRENT_INSERT)に、そのレコードが存在しないものとして取り扱うか。デフォルトはtrue|開発用のため将来的に削除される可能性あり|
|dev_point_read_concurrent_operation_as_not_found| ブール(true/false) | ポイントリード操作において並列して挿入されたレコードを検知した際(WARN_CONCURRENT_INSERT)に、そのレコードが存在しないものとして取り扱うか。デフォルトはtrue|開発用のため将来的に削除される可能性あり|
|lowercase_regular_identifiers| ブール(true/false) | SQLコンパイラがテーブル名などのシンボルを小文字に変換して扱うか。新コンパイラのみ有効。デフォルトはfalse||
|scan_block_size| 整数 | スキャンが他のタスクにスレッドを譲渡する前に処理するレコードの最大数。デフォルトは100|開発用のため将来的に削除/変更される可能性あり|
|scan_yield_interval| 整数 | スキャンが他のタスクにスレッドを譲渡する前に処理する最大時間（ms）。デフォルトは1|開発用のため将来的に削除/変更される可能性あり|
|dev_rtx_parallel_scan| ブール(true/false) | RTXの場合にparallel scanを有効にするか。デフォルトはfalse | 開発用のため将来的に削除/変更される可能性あり|
|dev_thousandths_ratio_check_local_first| 整数 | タスクスケジューラのワーカーがローカルタスクキューとstickyタスクキューのどちらを優先的にデキューするかを決めるパラメータ。デキューの実行1000回のうちローカルタスクキューを優先する回数(1以上1000未満)を指定する。デフォルトは100|開発用のため将来的に削除/変更される可能性あり|
|dev_direct_commit_callback| ブール(true/false) | shirakamiのコミット処理を行うスレッドが直接クラインアント通知までおこなうか。`commit_response` が `ACCEPTED` または `AVAILABLE` の場合のみ有効。デフォルトはfalse | 開発用のため将来的に削除/変更される可能性あり|
|scan_default_parallel| 整数 | スキャンタスクの最大並列実行数。デフォルトは1|開発用のため将来的に削除/変更される可能性あり|

## ipc_endpointセクション

セクション名
  - ipc_endpoint

対象コンポーネント
  - ipc_endpoint(tateyama)

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
|database_name | 文字列 | ipc_endpointに接続する際のurl名、デフォルトはtsurugi。 | この文字列は/dev/shmに作成されるファイル名のprefixとして使われる。
|threads | 整数 | ipc_endpointの最大同時接続数、デフォルトは104。
|datachannel_buffer_size | 整数 | resultsetのbuffer size、単位はKB、デフォルトは64。 | ipcで扱える最大のraw sizeはdatachannel_buffer_size-4B。
|max_datachannel_buffers | 整数 | 1セッションで同時使用可能なwriterの数。デフォルト値は256。 | このパラメータはセッションに対する上限値であり、システム（データベース・インスタンス）全体に対する上限値ではない。
|admin_sessions | 整数 | 管理コマンド（tgctl）用のセッション数。デフォルト値は1。 | threadsで指定する通常のセッション数上限とは別に用意する管理コマンド用のセッション数、指定可能な最大値は255。

## stream_endpointセクション

セクション名
  - stream_endpoint

対象コンポーネント
  - stream_endpoint(tateyama)

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
|port | 整数 | stream_endpointに接続する際のport番号、デフォルトは12345
|threads | 整数 | stream_endpointの最大同時接続数、デフォルトは104
|enabled | ブール(true/false) | stream_endpointを有効化 or 無効化してtsurugidbを起動する、デフォルトはfalse（無効化して起動する）

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
|zone_offset | 文字列 | セッションのデフォルトタイムゾーンオフセットを指定するISO8601規定の文字列 (`±[hh]:[mm]`形式 や `Z` など) 。デフォルトはUTCを示す文字列。 | 

## systemセクション

セクション名
  - system

対象コンポーネント
  - tateyama-bootstrap

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
|pid_directory | 文字列 | .pidファイル([プロセスの排他制御について](https://github.com/project-tsurugi/tateyama/blob/master/docs/process-mutex-ja.md)を参照)等のlockファイルを作成する一時ディレクトリを指定する、デフォルト値は/var/lock。 | 同一サーバ上で複数のtsurugidbインスタンスを稼働させる場合は、全tsurugidbインスタンスの構成ファイルについて本パラメータは同じ値に設定する必要がある。なお、lockファイルはサーバ死活監視のためtsubakuroのIPC接続からも参照される。

## glogセクション

セクション名
  - glog

対象コンポーネント
  - glog（logger）

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
|logtostderr | ブール(true/false) | ログファイルを出力せず、代わりに標準エラー出力にすべてのメッセージを出力する。デフォルト値はfalse。 |
|stderrthreshold | 整数 | 標準エラー出力に出力するログ重要度レベルの閾値。デフォルト値は2（ERRORに相当）。|
|minloglevel | 整数 | 出力する重要度ログレベル。デフォルト値は0（INFOに相当） |
|log_dir | 文字列 | ログファイルの出力先ディレクトリ。デフォルトは未指定（glogの既定のディレクトリに出力）。 |
|max_log_size | 整数 | ログファイルの最大サイズ（単位 MB）。デフォルト値は1800。 |
|v | 数値 | 出力する詳細ログのレベル。デフォルト値は0（出力しない）。|
|logbuflevel | 数値 | バッファリングするログ重要度レベルの閾値。重要度がこの値以下のログはバッファリングされる。デフォルト値は0（INFOに相当）。|

glogセクションで設定可能なパラメータのデフォルト値はglogのデフォルト値と同じ値です。

## ディレクトリの相対パス指定について
ディレクトリを指定するパラメータで相対パスが設定された場合、環境変数 TSURUGI_HOME からの相対パスとなります。
環境変数 TSURUGI_HOME が設定されていない場合はその相対パスは解決できないため、tsurugidbの起動は失敗します。
なお、すべてのディレクトリが絶対パスで指定されている場合は、tsurugidb起動に環境変数 TSURUGI_HOME の設定は関与しません。
