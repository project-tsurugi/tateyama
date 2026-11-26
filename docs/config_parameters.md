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
| index_restore_threads    | 整数 | 起動時のデータストアからのインデックス復元処理を行うスレッド数。デフォルトは物理コア数。0を指定すると、1スレッドで逐次に処理する。 | 物理コア数より大きい値(論理コア数など)を指定すると、かえって遅くなる。1スレッドあたり32MiBの読み取りバッファを使う。 |
| dev_iterator_based_scan  | ブール(true/false) | scanの挙動をイテレータベースとするか。デフォルトはfalse。 | 開発用のため将来的に削除/変更される可能性あり |

## datastoreセクション

セクション名
  - datastore

対象コンポーネント

  - データストア
  - トランザクショナルKVS(shirakami)

|                  パラメーター名 | 型 | 値                   | 備考                                                                                                                                                           |
|-------------------------:| :---: |:--------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------|
|             log_location | 文字列 | ログを保存するディレクトリへのパス | 必須パラメーターであり、設定がない場合はDBが起動に失敗する |
|  recover_max_parallelism | 整数 | 起動時のリカバリと、オンラインコンパクションで、ログをソートするスレッド数。デフォルトは 8 | 1 スレッドがソート用のバッファに最大 256 MiB のメモリを使うため、デフォルトでは約 2 GiB のメモリを使う。起動時だけでなく、運用中のオンラインコンパクションでも同じ量を使う。メモリの少ない環境では値を下げる。1 未満の値を指定した場合は DB が起動に失敗する |

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
|default_partitions | 整数 | 並列化可能な関係演算子の実行においてデータ分割を行う際のパーティション数。デフォルトは5。| |
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
|dev_thousandths_ratio_check_local_first| 整数 | タスクスケジューラのワーカーがローカルタスクキューとstickyタスクキューのどちらを優先的にデキューするかを決めるパラメータ。デキューの実行1000回のうちローカルタスクキューを優先する回数(1以上1000未満)を指定する。デフォルトは100|開発用のため将来的に削除/変更される可能性あり|
|dev_direct_commit_callback| ブール(true/false) | shirakamiのコミット処理を行うスレッドが直接クラインアント通知までおこなうか。`commit_response` が `ACCEPTED` または `AVAILABLE` の場合のみ有効。デフォルトはfalse | 開発用のため将来的に削除/変更される可能性あり|
|scan_default_parallel| 整数 | スキャンタスクの最大並列実行数。デフォルトは4。| 開発用のため将来的に削除/変更される可能性あり|
|dev_inplace_teardown| ブール(true/false) | ジョブの終了処理(teardown)を現在のスレッドで行いタスクの作成をバイパスするか。デフォルトはtrue | 開発用のため将来的に削除/変更される可能性あり|
|dev_inplace_dag_schedule| ブール(true/false) | プランスケジューラ(dag controller)の状態遷移を現在のスレッドで行いタスクの作成をバイパスするか。デフォルトはtrue | 開発用のため将来的に削除/変更される可能性あり|
|enable_join_scan| ブール(true/false) | インデックスを用いた結合処理において、全キー列のデータが利用可能でない場合に、その一部(プレフィックス)を利用して候補をスキャンして探索する演算子`join_scan` を使用可能にするか。`enable_index_join=true` の場合のみ有効。デフォルトはtrue | 開発用のため将来的に削除/変更される可能性あり|
|dev_rtx_key_distribution| 文字列 | RTXのparallel scanにおいて、キー範囲を分割する手法を指定。次のいずれかから選択 (`simple`, `uniform`, `sampling`) 。 `scan_default_parallel > 0` の場合のみ有効。デフォルトは uniform|開発用のため将来的に削除/変更される可能性あり|
|dev_enable_blob_cast| ブール(true/false) | BLOB/CLOBのキャストを可能にするか。デフォルトはtrue | 開発用のため将来的に削除/変更される可能性あり|
|max_result_set_writers | 整数 | writer resultsetの最大数。デフォルトは64。|この値は1以上256以下でなければならない|
|dev_core_affinity| ブール(true/false) | タスクスケジューラのワーカースレッドを(論理)CPUコアに割当てるか。デフォルトはfalse。trueの場合は `dev_initial_core` を先頭にシーケンシャルに割当てられる。`dev_force_numa_node` が未指定かつ `dev_assign_numa_nodes_uniformly=false` の場合のみ有効 | 開発用のため将来的に削除/変更される可能性あり|
|dev_initial_core | 整数 | CPUコア割当てをする際の先頭のCPUコア番号 (0-origin)。 `dev_core_affinity=true` の場合のみ有効。デフォルトは1。|開発用のため将来的に削除/変更される可能性あり|
|dev_assign_numa_nodes_uniformly| ブール(true/false) | タスクスケジューラのワーカースレッドをNUMAノードに均等に割当てるか。デフォルトはfalse。`dev_core_affinity=false` かつ `dev_force_numa_node` が未指定の場合のみ有効。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_force_numa_node | 整数 | タスクスケジューラのワーカースレッドを特定のNUMAノードへ割当てるか。ノード番号 (0-origin)を指定された場合そのノードですべてのワーカーが稼働する。`dev_core_affinity=false` かつ `dev_assign_numa_nodes_uniformly=false` の場合のみ有効。デフォルトは未指定 (ノード割当てなし)。|開発用のため将来的に削除/変更される可能性あり|
|dev_enable_disjunction_range_hinting| ブール(true/false) | `OR` を含む条件からカラムの範囲を抽出するか。デフォルトはtrue。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_apply_max_polls | 整数 | apply演算子がUDTFの結果レコードを受け取る際、レコードの準備が完了していない場合にイールド前に行うポーリング回数の最大値。デフォルトは0。|開発用のため将来的に削除/変更される可能性あり|
|dev_enable_maintenance_thread| ブール(true/false) | SQL実行エンジンがメンテナンススレッドを起動してストレージのバックグラウンド処理等に使用するか。デフォルトは true 。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_maintenance_interval_ms | 整数 | SQL実行エンジンが起動するメンテナンススレッドが一時停止した後に再開するまでの間隔(ms)。デフォルトは 100 。|開発用のため将来的に削除/変更される可能性あり|
|dev_enable_truncate| ブール(true/false) | TRUNCATE TABLE文をサポートするか。デフォルトは true。  | 開発用のため将来的に削除/変更される可能性あり|
|dev_enable_scan_limit| ブール(true/false) | TOP-N最適化のため、SQLコンパイラがscanに付与する件数制限を使用するか(SQLコンパイラにruntime_feature::scan_limitを渡す)。デフォルトは true。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_enable_ordered_scan| ブール(true/false) | TOP-N最適化のため、SQLコンパイラが指定した向きの順序でscanが出力するようにするか(runtime_feature::ordered_scanを渡す)。デフォルトは true。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_enable_ordered_forward| ブール(true/false) | TOP-N最適化のため、forward exchangeで入出力の順序を保持するか(runtime_feature::ordered_forwardを渡す)。`dev_enable_ordered_scan=true` の場合のみ有効。デフォルトは true。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_enable_backward_scan| ブール(true/false) | TOP-N最適化のため、SQLコンパイラが逆方向を指定したscanでインデックスを逆順に読むか(runtime_feature::backward_scanを渡す)。`dev_enable_scan_limit=true` または `dev_enable_ordered_scan=true` の場合のみ有効。デフォルトは true。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_enable_ties_ordered_by_primary_key| ブール(true/false) | TOP-N最適化のため、セカンダリインデックスのキーが同じエントリが主キーの順に並ぶことをSQLコンパイラに通知するか(index_feature::ties_ordered_by_primary_keyを付与する)。デフォルトは true。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_enable_apply_preserves_order| ブール(true/false) | TOP-N最適化のため、apply演算子が入力レコードの順序で出力することをSQLコンパイラに通知するか(runtime_feature::apply_preserves_orderを渡す)。デフォルトは true。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_enable_early_stop| ブール(true/false) | 件数制限のあるforward exchangeが件数分のレコードを受け取った際に上流の処理を早期に終了させるか。デフォルトは true。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_scan_impl| 文字列 | scan演算子が使用するトランザクションエンジンのscanの実装。`auto`(SQL実行エンジンが文に応じて選択)、`legacy`(常に従来のscan)、`iscan`(常にイテレーターベースのscan)のいずれか。デフォルトは `auto`。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_index_equivalent_selectivity| 実数 | SQLコンパイラがインデックスを選択する際の見積りで、インデックスキーに対する等値条件1つあたりの選択率の係数。0より大きく1以下。デフォルトは 0.1。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_index_full_bound_selectivity| 実数 | SQLコンパイラがインデックスを選択する際の見積りで、インデックスキーに対する両端のある範囲条件の選択率の係数。0より大きく1以下。デフォルトは 0.3。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_index_half_bound_selectivity| 実数 | SQLコンパイラがインデックスを選択する際の見積りで、インデックスキーに対する片端だけの範囲条件の選択率の係数。0より大きく1以下。デフォルトは 0.6。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_index_selectivity_decay| 実数 | SQLコンパイラがインデックスを選択する際の見積りで、複合キーの後ろのキーに対する条件の選択率の係数を弱める度合い。0以上1以下。1の場合はキーの列を互いに独立とみなす。デフォルトは 1.0。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_index_indirect_access_weight| 実数 | SQLコンパイラがインデックスを選択する際に、プライマリインデックスを引き直すセカンダリインデックスの読み方に掛ける重み。0より大きい。デフォルトは 2.0。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_index_sort_cost| 実数 | SQLコンパイラがインデックスを選択する際に、下流が要求する順序を持たない候補に加える並べ替えの費用の係数(1行を読む費用との比)。0以上。`dev_enable_ordered_scan=true` の場合のみ有効。デフォルトは 0.1。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_index_max_early_stop_limit_count| 整数 | SQLコンパイラが `ORDER BY ... LIMIT n` の文で、見積りと比べずに順序を満たすインデックスを選ぶ n の上限。0以上。`dev_enable_scan_limit=true` の場合のみ有効。デフォルトは 10000。 | 開発用のため将来的に削除/変更される可能性あり|
|dev_table_metadata_lock_shards| 整数 | 表のメタデータ(表とインデックスの定義)を参照する際の、読み手のロックの分割数。1以上。省略または値が空の場合はSQLコンパイラの既定値(現在は16)。 | 開発用のため将来的に削除/変更される可能性あり|

## udfセクション

セクション名
  - udf

対象コンポーネント
  - UDFサービス・リソース(jogasaki)

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
| plugin_directory | String | UDF プラグイン (.so ファイル) を配置するディレクトリへのパス。デフォルトは $TSURUGI_HOME/var/plugins/ | |
| endpoint | String | Tsurugi Database と UDF サーバ間の通信に利用する gRPC サーバのエンドポイント。`\|` 区切りで複数指定可能。複数指定した場合はラウンドロビン方式で接続先を選択する。デフォルトは "dns:///localhost:50051" | UDF プラグイン設定ファイルで指定されている場合は、プラグイン側の設定を優先する。 |
| secure | ブール(true/false) | セキュアな gRPC 通信路の利用可否。`\|` 区切りで複数指定可能。1個の場合はすべての `endpoint` に適用し、複数の場合は `endpoint` と同数指定して位置ごとに対応させる。デフォルト値は `false`。 | UDF プラグイン設定ファイルで指定されている場合は、プラグイン側の設定を優先する。 |
| timeout | Integer | UDF サーバへの各 RPC 呼び出しに対するタイムアウト時間を秒単位で指定する。未指定の場合、タイムアウトは設定しない。 | 0 を指定した場合、タイムアウトを無効化する。 |

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
|allow_blob_privileged | ブール(true/false) | 特権モードでのBLOB利用可否。デフォルト値はtrue（利用可能）。 |

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
|allow_blob_privileged | ブール(true/false) | 特権モードでのBLOB利用可否。デフォルト値はfalse（利用不可）。 |

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
|authentication_timeout | 整数 | 認証有効期限 (秒), 0 で有効期限なし、デフォルト値は0  |

## systemセクション

セクション名
  - system

対象コンポーネント
  - tateyama-bootstrap

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
|pid_directory | 文字列 | .pidファイル([プロセスの排他制御について](https://github.com/project-tsurugi/tateyama/blob/master/docs/process-mutex-ja.md)を参照)等のlockファイルを作成する一時ディレクトリを指定する、デフォルト値は/var/lock。 | 同一サーバ上で複数のtsurugidbインスタンスを稼働させる場合は、全tsurugidbインスタンスの構成ファイルについて本パラメータは同じ値に設定する必要がある。なお、lockファイルはサーバ死活監視のためtsubakuroのIPC接続からも参照される。

## authenticationセクション

セクション名
  - authentication

対象コンポーネント
  - authentication(tateyama)

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
| enabled | ブール(true/false) | 認証機構を有効にするかどうか、デフォルト値は`false` |
| url | 文字列 | 認証サービスの URL、デフォルト値は`http://localhost:8080/harinoki` |
| request_timeout | 実数 | 認証サービスのタイムアウト (秒), `0` でタイムアウトなし、デフォルト値は`0` |
| administrators | 文字列 | 管理ユーザのリスト（`,`区切り）、`*`で全loginユーザが管理ユーザとなる、デフォルト値は`*` |

## grpc_serverセクション

セクション名
  - grpc_server

対象コンポーネント
  - grpc(tateyama)

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
|enabled | ブール(true/false) | gRPC サーバーを有効にするかどうか、デフォルト値は`false` |
|listen_address | 文字列 | サーバーが Listen 先のアドレスとポート、デフォルト値は`0.0.0.0:52345` |
|endpoint | 文字列 | リモートから見た gRPC サーバーのエンドポイント URI。UDF で複数の接続先を使用する場合は `\|` 区切りで複数指定可能。1個の場合はすべての UDF サーバー設定に適用し、複数の場合は `[udf] endpoint` と同数指定して位置ごとに対応させる。デフォルト値は `dns:///localhost:52345` | UDF プラグイン設定ファイルで指定されている場合は、プラグイン側の設定を優先する。 |
|secure | ブール(true/false) | gRPC サーバーのセキュアポートを有効にするかどうか、デフォルト値は`false` |
|fullchain_crt | 文字列 | フルチェイン証明書ファイルのパス、デフォルト値はなし（`secure=true`の場合は設定必須） | gRPC サーバーのセキュアポートを有効にする場合のみ使用
|server_key | 文字列 | サーバ鍵ファイルのパス、デフォルト値はなし（`secure=true`の場合は設定必須） | gRPC サーバーのセキュアポートを有効にする場合のみ使用

## distributed_query_coordinatorセクション

セクション名
  - distributed_query_coordinator

対象コンポーネント
  - SQLサービス・リソース(jogasaki)

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
|enabled | ブール(true/false) | 分散クエリのcoordinatorを有効にするかどうか、デフォルト値は`false` | `true`の場合、`distributed_query_client`に1つ以上の接続先が必要。`false`の場合、`distributed_query_client`は使用・検証しない。 |

## distributed_query_remoteセクション

セクション名
  - distributed_query_remote

対象コンポーネント
  - SQLサービス・リソース(jogasaki)
  - grpc(tateyama)

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
|enabled | ブール(true/false) | 分散クエリのworkerとしてcatalog取得とprocess実行のRPCを受け付けるかどうか、デフォルト値は`false` | `true`の場合、`grpc_server.enabled=true`が必要。 |

## distributed_query_clientセクション

セクション名
  - distributed_query_client

対象コンポーネント
  - SQLサービス・リソース(jogasaki)

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
|name | 文字列 | remote server名、デフォルト値は空文字列（接続先なし）。 | `CREATE FOREIGN CATALOG`の`SERVER`で指定する名前。複数指定は`\|`区切り。Tsurugi 全体の識別子共通規則に従う。重複不可。`local`、および`__`で始まる名前は使用不可。 |
|uri | 文字列 | 接続先のURI、デフォルト値は空文字列（接続先なし）。 | `dns:///host:port`または`unix:///絶対パス`。複数指定は`\|`区切り。`name`と同数・同順で指定。 |
|secure | 文字列 | 接続先との通信にTLSを使用するかどうか（`true`/`false`）、デフォルト値は`false` | 指定する場合は`name`と同数・同順の`\|`区切りリストとする。省略時は全接続先に既定値を適用。UNIXドメインソケットでは`false`とする。 |
|root_cert | 文字列 | TLSの検証に使用するルート証明書、デフォルト値は`grpc_default` | `grpc_default`はgRPC既定のルート証明書を使用。それ以外はPEM形式のX.509証明書ファイルのパス。相対パスは`TSURUGI_HOME`を基準とする。指定する場合は`name`と同数・同順の`\|`区切りリストとする。省略時は全接続先に既定値を適用。`secure=false`の場合は使用しない。 |

## blob_relayセクション

セクション名
  - blob_relay

対象コンポーネント
  - blob_relay(data-relay-grpc/server)

|パラメーター名 | 型 | 値 |備考|
|---:| :---: | :--- |---|
|enabled | ブール(true/false) | BLOB 中継サービスを有効にするかどうか、デフォルト値は`true` |
|session_store | 文字列 | セッションストレージのルートディレクトリ、デフォルト値は`var/blob/sessions`（相対パス） |
|session_quota_size | 整数 | セッションストレージのクウォータサイズ、未指定の場合は無制限、デフォルト値は未指定 |
|local_enabled | ブール(true/false) | ファイルシステムを利用したデータ転送を有効にするかどうか、デフォルト値は`true` |
|local_upload_copy_file | ブール(true/false) | ファイルシステムを利用したアップロード時に元ファイルをコピーするかどうか、デフォルト値は`false` |
|stream_chunk_size | 整数 | gRPC ストリーミングにおいて、ダウンロード時にチャンク化してデータを転送する場合のチャンクサイズ (バイト)、デフォルト値は`1048576` |
|dev_accept_mock_tag | ブール(true/false) | Get操作において、mocktag(0xffffffffffffffff)を受理するか否か、デフォルト値は`false` |

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
