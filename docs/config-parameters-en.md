# Configuration file parameters

The tsurugi configuration parameters can be set in the following ini file format. 

```
[section]
parameter=value
```

Below is a list of parameters supported for each section.

## cc section

Section name
  - cc

Target component
  - Transactional KVS (shirakami)

| Parameter name | Type | Value | Remarks |
|---:| :---: | :--- |---|
| epoch_duration  | Integer | Length of the epoch (us). The default is 3000. |
| waiting_resolver_threads | Integer | Number of threads that process the waiting and pre-commit of the LTXs in the waiting list. Default is 2. |
| index_restore_threads | Integer | Number of threads that process index recovery from datastore on startup. Default is 4. |

## datastore section

Section name
  - datastore

Target component
  - Data store
  - Transactional KVS (shirakami)

| Parameter name | Type | Value | Remarks |
|---:| :---: | :--- |---|
| log_location | String | Path to the directory where the log is stored | This is a required parameter, and if it is not set, DB fails to start. |
| recover_max_parallelism | Integer | Number of concurrencies at the time of recovery. The default is 8. | This is for development and may be deleted/changed in the future. |

## sql section

Section name
  - sql

Target component
  - SQL Service Resource (jogasaki)


| Parameter name | Type | Value | Remarks |
|---:| :---: | :--- |---|
| thread_pool_size | Integer | Number of threads used by the task scheduler in the SQL service. The default value is set according to the environment by the following formula. MIN( <default worker coefficient> * <number of physical cores>, <maximum default worker count> ) If the result is less than 1, it is set to 1. Here, the default worker coefficient = 0.8, and the maximum default worker count = 32. |
| enable_index_join | Boolean (true/false) | Whether to use index-based join processing for performance improvement. The default value is true. | This is for development and may be deleted in the future. |
| stealing_enabled | Boolean (true/false) | Whether the scheduler steals tasks to utilize idle CPU cores. The default value is true. |
| default_partitions | Integer | Number of partitions when data is divided for parallelizable relational operators. The default value is 5. | The value must be equal to or less than `max_result_set_writers` |
| use_preferred_worker_for_current_thread | Boolean (true/false) | Whether the scheduler uses a fixed worker for each thread that submits a task. The default is true. | This is for development and may be deleted in the future. |
| stealing_wait | Integer | Coefficient of the number of times the task queue is checked before the worker of the task scheduler performs stealing. The worker checks its task queue more times by <number of workers> * <stealing_wait>. The default value is 1. | This is for development and may be deleted/changed in the future. |
| task_polling_wait | Integer | Time (us) the worker of the task scheduler waits until the next polling when it fails to check its task queue or steal candidates. The default value is 0. | This is for development and may be deleted/changed in the future. |
| enable_hybrid_scheduler | Boolean (true/false) | Whether to use the hybrid scheduler. The default value is true. | This is for development and may be deleted in the future. |
| lightweight_job_level | Integer | Threshold to define jobs that end quickly. Jobs with a level equal to or below this value are classified as jobs that end quickly and are candidates to be executed by the request thread. If 0 is specified, all jobs are treated as not ending quickly. The default value is 0. This is effective only when enable_hybrid_scheduler=true. The job level is determined by the executed statement. For details, see [`jogasaki::plan::statement_work_level_kind`](https://github.com/project-tsurugi/jogasaki/blob/497de010a018149cb22edc4ce780ab12734bc409/src/jogasaki/plan/statement_work_level.h#L24) | This is for development and may be deleted/changed in the future. |
| busy_worker | Boolean (true/false) | Whether the worker of the task scheduler checks the task queue frequently. The default is false. | This is for development and may be deleted in the future. |
| watcher_interval | Integer | Interval (us) the conditional worker in the task scheduler waits until the next resumption. The default value is 1000. | This is for development and may be deleted/changed in the future. |
| worker_try_count | Integer | Number of times the worker of the task scheduler checks its task queue before suspension. This is effective only when busy_worker=false. The default value is 1000. | This is for development and may be deleted/changed in the future. |
| worker_suspend_timeout | Integer | Time (us) the worker of the task scheduler waits until it resumes after suspension. This is effective only when busy_worker=false. The default value is 1000000. | This is for development and may be deleted/changed in the future. |
| commit_response | String | Default value for commit waiting. Select one of the following: (ACCEPTED, AVAILABLE, STORED, PROPAGATED). The default is STORED. | By explicitly specifying it at commit from the client, the above setting can be overwritten for each transaction. |
| dev_update_skips_deletion | Boolean (true/false) | If the updated column of the UPDATE statement is not a primary key or an indexed column, skip record deletion as much as possible. The default value is false. | This is for development and may be deleted in the future. |
| dev_profile_commits | Boolean (true/false) | Whether to output profiling information for commit processing. The default value is false. | This is for development and may be deleted in the future. |
| dev_return_os_pages | Boolean (true/false) | Whether to return pages to the OS when they are returned to the memory management page pool. The default value is false. | This is for development and may be deleted in the future. |
| dev_omit_task_when_idle | Boolean (true/false) | Whether to skip scheduling tasks for durability callback processing when there are no durable waiting transactions. The default value is true. | This is for development and may be deleted in the future. |
| plan_recording | Boolean (true/false) | Whether to output the `stmt_explain` item (SQL execution plan) in the Altimeter event log. The default value is false. |
| dev_try_insert_on_upserting_secondary | Boolean (true/false) | Whether to execute INSERT before INSERT OR REPLACE for tables with secondary indexes. The default value is true. | This is for development and may be deleted in the future. |
| dev_scan_concurrent_operation_as_not_found | Boolean (true/false) | Whether to treat records detected as concurrently inserted during scan operations (WARN_CONCURRENT_INSERT) as non-existent. The default value is true. | This is for development and may be deleted in the future. |
| dev_point_read_concurrent_operation_as_not_found | Boolean (true/false) | Whether to treat records detected as concurrently inserted during point read operations (WARN_CONCURRENT_INSERT) as non-existent. The default value is true. | This is for development and may be deleted in the future. |
|lowercase_regular_identifiers| Boolean (true/false) | Whether to lowercase the identifiers such as table names. This configuration is available only on new SQL compiler. The default value is false.||
|scan_block_size| Integer | Max records processed by scan operator before yielding to other tasks. The default is 100. | This is for development and may be deleted in the future. |
|scan_yield_interval| Integer | Max time (ms) processed by scan operator before yielding to other tasks. The default is 1. | This is for development and may be deleted in the future. |
|dev_thousandths_ratio_check_local_first| Integer | This parameter defines whether the worker of the task scheduler gives priority to dequeue the local task queue or the sticky task queue. Specify the number of times (greater than or equal to 1 and less than 1000) to give priority to the local task queue out of 1000 dequeue executions. The default is 100. |This is for development and may be deleted in the future. |
|dev_direct_commit_callback| Boolean (true/false) | Whether commit processing thread in shirakami directly notifies client of commit result. Effective only when `commit_response` is either `ACCEPTED` or `AVAILABLE`. The default value is false. | This is for development and may be deleted in the future. |
|scan_default_parallel| Integer | Maximum Degree of Parallelism for Scan Tasks. The default is 4. | The value must be equal to or less than `max_result_set_writers`. This is for development and may be deleted in the future. |
|dev_inplace_teardown| Boolean (true/false) | Whether to process job completion (teardown) on current thread without creating new task. The default value is true. | This is for development and may be deleted in the future. |
|dev_inplace_dag_schedule| Boolean (true/false) | Whether to process state management by plan scheduler (dag controller) on current thread without creating new task. The default value is true. | This is for development and may be deleted in the future. |
|enable_join_scan| Boolean (true/false) | Whether to enable `join_scan`, an operator that scans and searches for candidates using a portion (prefix) of the data in an index-join process, when the data for the whole key columns are not available. Effective only if `enable_index_join=true`. The default value is true. | This is for development and may be deleted in the future. |
|dev_rtx_key_distribution| String | Specifies the method used to split key range used by RTX parallel scan. Choose one of the following `simple`, `uniform`, or `sampling`. Valid only if `scan_default_parallel > 0`. Default is `uniform` | This is for development and may be deleted in the future. |
|dev_enable_blob_cast| Boolean (true/false) | Whether to enable cast expression with BLOB/CLOB. The default value is true.| This is for development and may be deleted in the future.|
|max_result_set_writers| Integer | The maximum number of writers for a result set. The default value is 64. | The value must be equal to or less than 256. |
| dev_core_affinity | Boolean (true/false) | Determines whether the task scheduler’s worker threads are pinned to (logical) CPU cores. Default is `false`. When `true`, cores are assigned sequentially starting from `dev_initial_core`. Effective only if `dev_force_numa_node` is not specified and `dev_assign_numa_nodes_uniformly = false`. | This is for development and may be deleted in the future. |
| dev_initial_core | Integer | First CPU core number (0-based) when assigning cores. Effective only when `dev_core_affinity = true`. Default is `1`. | This is for development and may be deleted in the future. |
| dev_assign_numa_nodes_uniformly | Boolean (true/false) | Uniformly distributes the task scheduler’s worker threads across NUMA nodes. Default is `false`. Effective only when `dev_core_affinity = false` and `dev_force_numa_node` is not specified. | This is for development and may be deleted in the future. |
| dev_force_numa_node | Integer | Forces all task scheduler worker threads onto a specific NUMA node. Specify the node number (0-based). Effective only when `dev_core_affinity = false` and `dev_assign_numa_nodes_uniformly = false`. Default is *not specified* (no node affinity). | This is for development and may be deleted in the future. |

## ipc_endpoint section

Section name
  - ipc_endpoint

Target component  
  - ipc_endpoint(tateyama)    

| Parameter name | Type | Value | Remarks |
|---:| :---: | :--- |---|
| database_name | String | URL to connect to ipc_endpoint. The default value is "tsurugi". | This string is used as a prefix for the file created in /dev/shm. |
| threads | Integer | Maximum number of simultaneous connections to ipc_endpoint. The default value is 104. |
| datachannel_buffer_size | Integer | Buffer size of resultset in KB. The default value is 64. | The maximum raw size that can be handled by ipc is datachannel_buffer_size-4B. |
| max_datachannel_buffers | Integer | Number of writers that can be used simultaneously in one session. The default value is 256. | This parameter is the upper limit for the session, not the entire system (database instance).
| admin_sessions | Integer | Number of sessions for management commands (tgctl). The default value is 1. | The maximum number of sessions for management commands that can be specified is 255, which is separate from the normal maximum number of sessions specified in threads.
| allow_blob_privileged | Boolean (true/false) | Whether BLOBs are allowed in privileged mode or not. The default value is true(allowed). |

## stream_endpoint section

Section name
  - stream_endpoint 

Target component
  - stream_endpoint(tateyama)

| Parameter name | Type | Value | Remarks |
|---:| :---: | :--- |---|
| port | Integer | Port number to connect to stream_endpoint. The default value is 12345. |
| threads | Integer | Maximum number of simultaneous connections to stream_endpoint. The default value is 104. |
| enabled | Boolean (true/false) | Enable or disable stream_endpoint when tsurugidb starts. The default value is false (disabled at start).
| allow_blob_privileged | Boolean (true/false) | Whether BLOBs are allowed in privileged mode or not. The default value is false(not allowed). |

## session section

Section name
  - session 

Target component
  - All tateyama components that use sessions

| Parameter name | Type | Value | Remarks |
|---:| :---: | :--- |---|
| enable_timeout | Boolean (true/false) | Enable or disable automatic session timeout. The default value is true. |
| refresh_timeout | Integer | Lifetime extension time (seconds) of the session by communication. The default value is 300. |
| max_refresh_timeout | Integer | Maximum value of the lifetime extension time of the session by explicit request (seconds). The default value is 10800. |
| zone_offset | String | An ISO8601-defined string specifying the default time zone offset for the session (in the format `±[hh]:[mm]`, `Z`, etc.) The default value is a string indicating UTC. | 
| authentication_timeout | Integer | Authentication expiration time (seconds), 0 for no expiration. The default value is 0.  |


## system section

Section name
  - system  

Target component
  - tateyama-bootstrap

| Parameter name | Type | Value | Remarks |
|---:| :---: | :--- |---|
|pid_directory | String | Specify the temporary directory to create lock files such as .pid files (see [process mutex](https://github.com/project-tsurugi/tateyama/blob/master/docs/process-mutex-ja.md)). The default value is /var/lock. | If you run multiple tsurugidb instances on the same server, you need to set the same value for this parameter in the configuration file of all tsurugidb instances. The lock files are also accessed from IPC connections from tsubakuro and others in order to monitor the server's dead/alive status. |

## authentication section

Section name
  - authentication(tateyama)

Target component
  - authentication

| Parameter name | Type | Value | Remarks |
|---:| :---: | :--- |---|
| enabled | Boolean(true/false) | Whether to enable the certification mechanism. The default value is true. |
| url | String | URL of the authentication service. The default value is 'http://localhost:8080/harinoki' |
| request_timeout | Number | Authentication service timeout in seconds, 0 for no timeout. The default value is 0. |

## glog section

Section name
  - glog  

Target component

  - glog (logger)

| Parameter name | Type | Value | Remarks |
|---:| :---: | :--- |---|
|logtostderr | Boolean (true/false) | Output all messages to standard error output instead of the log file. The default value is false. |
|stderrthreshold | Integer | Threshold of log level to output to standard error output. The default value is 2 (equivalent to ERROR). |
|minloglevel | Integer | Log level to output. The default value is 0 (equivalent to INFO). |
|log_dir | String | Directory where log files are output. The default is unspecified (output to the default directory of glog). |
|max_log_size | Integer | Maximum size of log file (unit MB). The default value is 1800. |
|v | Number | Level of detailed logs to output. The default value is 0 (do not output). |
|logbuflevel | Number | Threshold of log level to buffer. Logs with level below this value are buffered. The default value is 0 (equivalent to INFO). |

The default values of parameters that can be set in the glog section are the same as the default values of glog.

## specifying relative path for directory parameters

If a relative path is set in the parameter that specifies the directory, it is resolved as a relative path from the environment variable TSURUGI_HOME.
If the environment variable TSURUGI_HOME is not set, the relative path cannot be resolved, and the tsurugidb startup fails.
If all directories are specified by absolute paths, the setting of the environment variable TSURUGI_HOME does not affect the startup of tsurugidb.
