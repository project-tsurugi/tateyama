# endpoint_diag出力内容
2024.12.04
NT horikawa

## diagnostic出力内容
endpointの状態として下記の情報を出力する。

### セッション状態（記載内容はipc, tcp共通）
各セッションについて下記情報を出力する
* session id
* （処理中の）request-response状態
  * slot番号
  * request
    * service id requestを処理するserviceのcomponent id
    * local_id セッション内で一意となるように付与されるrequest id
    * payload（バイナリを16進ダンプ）
  * response
    * 状態
      * data_channel状態
        * body_head未送信
        * body_head送信済み（下記に分類、複数の状態が同時に該当することはない）
          * acquire_channel()未実施
          * acquire_channel()済（release_channel()未実施）
          * release_channel()済（acquire_channel()は成功している）
          * acquire_channel()に失敗した
          * release_channel()に失敗した
      * （bodyやerrorを送信したか否か）
        * bodyやerrorが送信されると、そのrequest-responseはセッションの管理対象から外れる（objectもdeleteされる）ので、これはrequest-response状態に表示されるか否かで判断する。

### 接続状態
* 次の接続要求に対して割り当てる予定のsession id
* acceptされず、pending状態になっている接続要求の数（ipcのみ）