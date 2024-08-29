# endpoint メトリクスプロバイダ
2024.08.26
NT horikawa

## この文書について
`tgctl dbstats show`で表示されるendpointのメトリクスを規定する。

## メトリクス項目の定義
### セッション数
* 項目名：session_count
* 定義：IPCまたはTCP endpointから接続しているセッション数。
  * セッション数には`tgctl dbstats show`を実行するためのセッションは含まない。
* 更新：セッション数の増減に応じて本メトリクス値は更新される。

### IPCバッファサイズ
* 項目名：ipc_buffer_size
* 定義：IPC通信用に割り当てた共有メモリ・サイズ、下式により計算する。
  * `9800 + (ipc_endpoint.threads * 112)` を4Kバイト単位に切り上げた値 + 
   `((ipc_endpoint.datachannel_buffer_size + 7700) * ipc_endpoint.max_datachannel_buffers + 28672) * 接続しているIPCセッション数`
    * `ipc_endpoint.`の付されたパラメータはtsurugi.iniで設定されている値。
    * 接続しているIPCセッション数には`tgctl dbstats show`を実行するためのIPC接続は含まない。
* 更新：IPC接続セッション数の増減に応じて本メトリクス値は更新される。
