# セッションと各種リソースのライフタイムについて

## 当文書について

トランザクション等のライフタイムを持つリソースはセッションと関連付けることで、そのライフタイムの完了(タイムアウト)を自動化することができる。当文書はその仕様について記述する。

## セッションによるリソースのライフタイム制御

リソースはセッションと関連付け(アタッチ)されることにより、ライフタイムがセッションによって制御される

* セッション内で開始したリソースは、セッションにアタッチされた状態となる
* セッションにアタッチされたリソースのライフタイムは、セッションのライフタイムに準ずる
  * セッションによるタイムアウト機能を使わずに、個別にリソースを破棄することも可能
* セッションは通信が行われなくなると一定時間（クライアント側で設定可能）で自動的にライフタイムが終了する(タイムアウト)
* セッションのライフタイムを明示的に延長させるコマンドも存在する (KeepAliveメッセージ)

* 一部のリソースは、明示的にセッションからデタッチすることができる
* セッションからデタッチされたリソースは、個別にライフタイムを指定できる
* セッションからデタッチされたリソースを利用するには、いずれかのセッションに再アタッチする必要がある
* セッションに再アタッチされたリソースのライフタイムは、そのセッションのライフタイムに準ずる

## セッションのライフタイムの更新方法

セッションは通信を行うか、明示的に指示することによりライフタイムの延長を行うことが可能である

* 通信によるライフタイムの延長: 既定値5分、設定上限30分
* 明示的にライフライムを延長: 最大3時間 (サーバ側で設定可能)、通常は1時間未満を想定
* 明示的なライフタイム延長後に通信を行った場合、明示的に指定されたライフタイム期限と通信によるライフタイム延長のうち期限が長い方を採用する
* 明示的にライフタイムを指定した場合、現在のライフタイムの期限によらず、指定したライフタイムの期限を採用する

(注) ライフタイム期限は「そこを過ぎた場合にタイムアウトによりリソースが完了・リリースされる可能性のある最小の時刻」を表しており、その期限が過ぎた際にすぐにリソースの完了が保証されるものではない。

## tsubakuro API for keep alive

クライアントから、セッションのライフタイム期限を明示的に更新する方法として、 tsubakuroのSessionに下記のメソッドが追加される。

```java
/**
 * Requests to update the session expiration time.
 * <p>
 * The resources underlying this session will be disposed after this session was expired.
 * To extend the expiration time, clients should continue to send requests in this session, or update expiration time explicitly by using this method.
 * </p>
 * <p>
 * If the specified expiration time is too long, the server will automatically shorten it to its limit.
 * </p>
 * @param time the expiration time from now
 * @param unit the time unit of expiration time
 * @return the future response of the request;
 *     it will raise {@link CoreServiceException} if request was failure
 * @throws IOException if I/O error was occurred while sending request
 */
FutureResponse<Void> updateExpirationTime(long time, TimeUnit unit) throws IOException;
```

## 対象リソース例

* セッションからデタッチ不可
  * トランザクション
  * バックアップ制御リソース
    * バックアップ処理の開始から終了の宣言の間に存在する仮想的なリソース

* セッションからデタッチ可能
  * リストア制御リソース？ (詳細はTBD)


