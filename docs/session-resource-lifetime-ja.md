# セッションと各種リソースのライフタイムについて

## 当文書について

トランザクション等のライフタイムを持つリソースはセッションと関連付けることで、そのライフタイムの完了(タイムアウト)を自動化することができる。当文書はその仕様について記述する。

## セッションによるリソースのライフタイム制御

リソースはセッションと関連付け(アタッチ)されることにより、ライフタイムがセッションによって制御される

* セッション内で開始したリソースは、セッションにアタッチされた状態となる
* セッションにアタッチされたリソースは、セッションのライフタイム終了時に自動的に破棄される
  * 個別にリソースを破棄することも可能
* セッションは通信が行われなくなると一定時間で自動的にライフタイムが終了する(タイムアウト)
* セッションのライフタイムを明示的に延長させるコマンドも存在する

* 一部のリソースは、明示的にセッションからデタッチすることができる
* セッションからデタッチされたリソースは、個別にライフタイムを指定できる
* セッションからデタッチされたリソースを利用するには、いずれかのセッションに再アタッチする必要がある
* セッションに再アタッチされたリソースは、当該セッションのライフタイム終了時に自動的に破棄される

## セッションのライフタイムの更新方法

セッションは通信を行うか、明示的に指示することによりライフタイムの延長を行うことが可能である

* 通信によるライフタイムの延長: 既定値5分 (サーバ側で設定可能)
* 明示的にライフライムを延長: 既定最大値3時間 (サーバ側で設定可能)、通常は1時間未満を想定
* 明示的なライフタイム延長後に通信を行った場合、明示的に指定されたライフタイム期限と通信によるライフタイム延長のうち期限が長い方を採用する
* 明示的にライフタイムを指定した場合、現在のライフタイムの期限によらず、指定したライフタイムの期限を採用する
  * ただし、ライフタイム期限は最小値しか保証しないので現在のライフタイムの期限よりも短くする設定は意味がないことがある
* サーバ側の設定で、ライフタイム機能を利用しない (セッションを自動でタイムアウトさせない) ことも可能
  * ライフタイム機能を利用しない場合、各種ライフタイム延長は無視される

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
FutureResponse<Void> updateExpirationTime(long time, @Nonnull TimeUnit unit) throws IOException;
```

(注) デタッチ可能なリソースのAPIはTBD
- セッションへデタッチ/アタッチするための関数
- デタッチされたリソースのライフタイム設定用関数など

## 対象リソース例

対象となるリソースの例をあげる。詳細はTBD

* セッションからデタッチ不可
  * (通常の)トランザクション
  * バックアップ制御リソース
    * バックアップ処理の開始から終了の宣言の間に存在する仮想的なリソース

* セッションからデタッチ可能
  * 長時間処理を行うトランザクション
    * ETLのような時間がかかる処理において単一セッションが維持されることが期待できないトランザクション
  * リストア制御リソース？ (詳細はTBD)

## 設定 (案)

tateyama構成ファイルの[`session`セクション](https://github.com/project-tsurugi/tateyama/blob/master/docs/config_parameters.md#sessionセクション
)で、タイムアウト有無の制御や延長時間の設定を可能にする。