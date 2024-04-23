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
| enable_index_join | Boolean (true/false) | Whether to use index-based join processing for performance improvement. The default value is false. | This is for development and may be deleted in the future. |
| stealing_enabled | Boolean (true/false) | Whether the scheduler steals tasks to utilize idle CPU cores. The default value is true. |
| default_partitions | Integer | Number of partitions when data is divided for parallelizable relational operators. The default value is 5. |
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
| external_log_explain | Boolean (true/false) | Whether to output the `stmt_explain` item (SQL execution plan) in the Altimeter event log. The default value is true. |
| dev_try_insert_on_upserting_secondary | Boolean (true/false) | Whether to execute INSERT before INSERT OR REPLACE for tables with secondary indexes. The default value is true. | This is for development and may be deleted in the future. |
| dev_scan_concurrent_operation_as_not_found | Boolean (true/false) | Whether to treat records detected as concurrently inserted during scan operations (WARN_CONCURRENT_INSERT) as non-existent. The default value is true. | This is for development and may be deleted in the future. |
| dev_point_read_concurrent_operation_as_not_found | Boolean (true/false) | Whether to treat records detected as concurrently inserted during point read operations (WARN_CONCURRENT_INSERT) as non-existent. The default value is true. | This is for development and may be deleted in the future. |

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

## stream_endpoint section

Section name
  - stream_endpoint 

Target component
  - stream_endpoint(tateyama)

| Parameter name | Type | Value | Remarks |
|---:| :---: | :--- |---|
| port | Integer | Port number to connect to stream_endpoint. The default value is 12345. |
| threads | Integer | Maximum number of simultaneous connections to stream_endpoint. The default value is 104. |
| enabled | Boolean (true/false) | Enable or disable stream_endpoint when tsurugidb starts. The default value is true (enabled at start).

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

## system section

Section name
  - system  

Target component
  - tateyama-bootstrap

| Parameter name | Type | Value | Remarks |
|---:| :---: | :--- |---|
|pid_directory | String | Specify the temporary directory to create lock files such as .pid files (see [process mutex](https://github.com/project-tsurugi/tateyama/blob/master/docs/process-mutex-ja.md)). The default value is /var/lock. | If you run multiple tsurugidb instances on the same server, you need to set the same value for this parameter in the configuration file of all tsurugidb instances. |

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
