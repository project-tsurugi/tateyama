# 共有メモリ使用量について
2024-06-13 horikawa (NT)

Tsurugidb（tateyama）では、boost managed shared memory（以下、共有メモリ）により、クライアントプロセス（tsubakuro, tateyama-bootstrap, ogawayama, etc.）との間で情報の受け渡しを行っている。
このメモでは、共有メモリとして割り当てるメモリ容量を記す。

## 用途（種類）
共有メモリの用途は以下の通り。

* Tsurugidbプロセスの状態通知用
* IPC endpoint接続処理用
* IPC endpointで接続されるsessionの通信用

「Tsurugidbプロセスの状態通知」と「IPC endpoint接続処理」はTsurugidbに各1segment、「IPC endpointで接続されるsessionの通信」は、IPC接続されているセッション数分のsegmentが作成される。

「IPC endpointで接続されるsessionの通信」には、更に下記の用途がある。
* クライアントからのリクエスト・メッセージ転送用
* クライアントへのレスポンス・メッセージ転送用
* クライアントにデータチャネル経由で送るデータ転送用

これらは、「IPC endpointで接続されるsessionの通信」のために確保した共有メモリ内にデータ構造を作成して（メモリ領域を割り当てて）使用する。なお、「クライアントにデータチャネル経由で送るデータ転送用」のデータ構造は、不要になった時点で消去する。

## 各共有メモリの容量
「Tsurugidbプロセスの状態通知」と「IPC endpoint接続処理」に割り当てるメモリ容量は、IPC接続可能な最大セッション数（tsurigi.iniのipc_endpoint.threadsバラメータ）に依存する。本項では、そのパラメータをnと表記する。
また、各種の固定値は共有メモリの使用量を測定して求めた値である。

###  Tsurugidbプロセスの状態通知用
`initial_size + (n * per_size) +  initial_size / 2` を4Kバイト単位に切り上げた値
ここで、initial_sizeは640、per_sizeは8。

### IPC endpoint接続処理用
`initial_size + (n * per_size) +  initial_size / 2` を4Kバイト単位に切り上げた値
ここで、initial_sizeは720、per_sizeは112。

### IPC endpointで接続されるsessionの通信用
`(datachannel_buffer_size + data_channel_overhead) * max_datachannel_buffers + (request_buffer_size + response_buffer_size) + total_overhead`
ここで、datachannel_buffer_size, max_datachannel_buffersは、tsurigi.iniのipc_endpoint.datachannel_buffer_sizeとipc_endpoint.max_datachannel_buffersバラメータ、
data_channel_overheadは7700、request_buffer_sizeは4096, response_buffer_sizeは8192、total_overheadは16384。
