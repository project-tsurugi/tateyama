# 共有メモリ使用量について
2024-06-13 horikawa (NT)
2024-06-26 rev.1

Tsurugidb（tateyama）では、boost managed shared memory（以下、共有メモリ）により、クライアントプロセス（tsubakuro, tateyama-bootstrap, ogawayama, etc.）との間で情報の受け渡しを行っている。
このメモでは、共有メモリのライフサイクルと割り当てるメモリ容量を記す。

## 用途（種類）
共有メモリの用途は以下の通り。

* Tsurugidbプロセスの状態通知用
* IPC endpoint接続処理用
* IPC endpointで接続されるsessionの通信用

「Tsurugidbプロセスの状態通知」と「IPC endpoint接続処理」はTsurugidbに各1segment*)、「IPC endpointで接続されるsessionの通信」は、IPC接続されているセッション数分のsegmentが作成される。
*) segmentは共有メモリを確保する単位で、linuxでは/dev/shm下に作成されるファイルと1:1に対応している。

## 各共有メモリのライフサイクルと容量
「Tsurugidbプロセスの状態通知」と「IPC endpoint接続処理」に割り当てるメモリ容量は、IPC接続可能な最大セッション数（tsurigi.iniのipc_endpoint.threadsパラメータとipc_endpoint.admin_sessionsパラメータの和）に依存する。本項では、そのパラメータをnと表記する。また、各種の固定値は共有メモリの使用量を測定して求めた値である。

###  Tsurugidbプロセスの状態通知用
#### ライフサイクル
tsurugidb起動時（setup時）に1segment確保し、tsurugidbのshutdown時に開放する。

#### 容量
`initial_size + (n * 2* per_size) +  initial_size / 2` を4Kバイト単位に切り上げた値
ここで、initial_sizeは640、per_sizeは8。

### IPC endpoint接続処理用
#### ライフサイクル
tsurugidb起動時（setup時）に1segment確保し、tsurugidbのshutdown時に開放する。

#### 容量
`initial_size + (n * per_size) +  initial_size / 2` を4Kバイト単位に切り上げた値
ここで、initial_sizeは720、per_sizeは112。

### IPC endpointで接続されるsessionの通信用
#### ライフサイクル
IPCセッション接続時にそのセッション用の共有メモリを1segment確保し、セッション終了時に開放する。

#### 容量
`(datachannel_buffer_size + data_channel_overhead) * max_datachannel_buffers + (request_buffer_size + response_buffer_size) + total_overhead`
ここで、固定値はdata_channel_overhead = 7700、request_buffer_size = 4096, response_buffer_size = 8192、total_overhead = 16384。
configurationで設定するパラメータは、datachannel_buffer_size, max_datachannel_buffers。各々、tsurigi.iniのipc_endpoint.datachannel_buffer_sizeとipc_endpoint.max_datachannel_buffersパラメータとして設定した値。

#### 補足
セッション用共有メモリを使う際は、その中に下記用途のデータ構造を作成する。このデータ構造作成および削除操作は、セッション用共有メモリ容量に影響しない（データ構造作成は、前項に記した容量を上限として行われ、それを超える場合はエラーとなる）。
* クライアントからのリクエスト・メッセージ転送用
* クライアントへのレスポンス・メッセージ転送用
* クライアントにデータチャネル経由で送るデータ転送用

最初の２つのデータ構造は、セッション用共有メモリの作成時に作成され、以降、削除されることはない（セッション用共有メモリ削除によって消失する）。
一方、データチャネル経由で送るデータ転送用のデータ構造は、データチャネルを使うデータ転送を行う際（例；selectのresult setを転送する際）に作成され、そのデータ転送が完了したら消去される。但し、このでデータ構造の作成や消去は、上述した通り、セッション用共有メモリの容量には影響しない。
