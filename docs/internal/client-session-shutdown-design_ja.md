# クライアントセッションの安全なシャットダウンのデザイン

* 2024-02-13 arakawa (NT)

## この文書について

* セッションの安全なシャットダウンを Tsubakuro から選択的に可能にする機能のデザインを共有

## コンセプト

* セッションオブジェクトを破棄する `Session.close()` のほかに、安全にセッションを終了し、その終了を待機する仕組みを導入する
  * `Session.close()` はセッションオブジェクトを破棄し、呼び出し元に速やかに制御を戻す
  * 上記とは別に、 `Session.shutdown()` というメソッドを提供し、こちらは安全にセッションを終了し、その終了を待機できるようにする
* セッションの安全な終了は、[セッションをコントロールするCLIのデザイン]の graceful/forceful shutdown のいずれかを行う
  * `Session.shutdown()` を呼び出すと、上記の shutdown を行ったうえで、その完了を待ち合わせることになる
    * ただし、 `Session.shutdown()` 自体がTsubakuroスタイルの非同期呼び出しなので、実際にはその戻り値の future 経由で待ち合せる
  * `Session.shutdown()` のリクエストがサーバ側に受理されたら、そのセッションは常にシャットダウン状態に移行する
    * シャットダウン状態になった以降は、ほとんどの新しいリクエストを受け付けず、基本的にシャットダウンは成功する
    * ただし、セッション中のリクエストがストールしていた場合、そのリクエストが終了せずに無限に待ち合わせる場合はある
* いずれにしても、従来の `Session.close()` の呼び出しは必要
  * `Session.close()` はクライアント側リソースの完全な破棄を含む
  * ただし、 `Session.shutdown()` とはことなり、迅速に処理が行われることが期待される

[セッションをコントロールするCLIのデザイン]:https://github.com/project-tsurugi/tateyama/blob/master/docs/internal/session-control-design_ja.md

## Java API

### `Session` オブジェクト

```java
public interface Session extends ServerResource {
    // ...

    /**
     * Request to shutdown the current session and wait for the running requests were finished.
     * <p>
     * Note that, this only send a shutdown request to the server, and some resources underlying this object may
     * be still living (e.g. JMX resources).
     * Please invoke {@link #close()} after this operation to dispose such the resources.
     * </p>
     * @param type the shutdown type
     * @return the future response of the request;
     *     it will raise {@link CoreServiceException} if request was failure
     * @throws IOException if I/O error was occurred while sending request
     * @see #close()
     */
    FutureResponse<Void> shutdown(@Nonnull ShutdownType type) throws IOException;

    // ...

    /**
     * Disposes the current session.
     * <p>
     * This may not wait for complete the ongoing requests, and it may cause the requests may still be in progress
     * after disconnected from the session.
     * You can use {@link #shutdown(ShutdownType)} to safely close this session
     * with waiting for complete the ongoing requests, if any.
     * </p>
     * <p>
     * After this operation, you cannot send any requests via this session,
     * including service clients attached to this session.
     * </p>
     * <p>
     * If this session is already disposed, this only cleanup the client-side resources corresponding to this session,
     * and never send any requests to the server.
     * </p>
     * @throws ServerException if error was occurred while disposing this session
     * @throws IOException if I/O error was occurred while disposing this session
     * @throws InterruptedException if interrupted while disposing this session
     * @see #shutdown(ShutdownType)
     */
    @Override
    void close() throws ServerException, IOException, InterruptedException;

    // ...
}
```

* 備考
  * `shutdown()` に成功しても、クライアント側のリソースを一部残しておく必要がある
    * JMX リソースなどの監視に関するものなど
    * 逆に、 keep-alive 用のスレッドなどは適切に終了させないとエラーが発生する可能性がある
      * ただし、 graceful shutdown の場合、リクエストの完了を待ってから keep-alive を解除しないと、タイムアウトからの forceful shutdown への格上げなどが行われる可能性がある (graceful shutdown の処理中にも keep-alive 系のみは受け入れる)
  * 実装上、 `Session.close()` は先行する `Session.shutdown()` によってセッションが終了済みであっても、エラーなく動作する必要がある

### `ShutdownType` 列挙

```java
/**
 * Represents kind of shutdown type.
 */
public enum ShutdownType {

    /**
     * Waits for the ongoing requests and safely shutdown the session.
     * <p>
     * This may wait for complete the current running requests, and then shutdown this session.
     * </p>
     */
    GRACEFUL,

    /**
     * Cancelling the ongoing requests and safely shutdown the session.
     * <p>
     * This request will first the cancelling the ongoing requests.
     * Once each request detects the cancellation, it will discontinue the subsequent work.
     * Finally, this operation may wait for complete or cancel the requests, and then shutdown this session.
     * </p>
     */
    FORCEFUL,
}
```

## Protocol Buffers

* source
  * `tateyama/proto/core/*.proto`
* 宛先は `router` を想定
  * `sessions` と一部重複した内容になるが、「現在のセッション」特有の仕組みになるため別途用意する
  * また、 `sessions` の `SessionShutdown` は終了の待ち合わせを行わない点で挙動が少し異なる
  * `router` であるのは、このあたりのメッセージが多少特殊であるため、まとめておきたい
    * 「シャットダウンリクエスト以外のリクエスト」がすべて完了するとシャットダウンできる
      * シャットダウンだけは特別扱いする必要がある
    * graceful shutdown が要求された後、一部のリクエスト以外は閉塞する
      * セッション有効期限の延長 (自動的に forceful に移行させないため)
      * forceful shutdown への移行 (リクエストが終了しない場合向け)
      * 上記2つはいずれも `router` での実現を想定している
    * `router` 以外であっても、特別扱いできるなら何でもよい
      * ワイヤごとに特別な扱いができる仕組みを用意するなど

### `ShutdownType` 列挙 (protobuf)

```proto
// kind of shutdown type.
enum request.ShutdownType {

    // The default shutdown type.
    SHUTDOWN_TYPE_NOT_SET = 0;

    // Waits for the ongoing requests and safely shutdown the session.
    GRACEFUL = 1;

    // Cancelling the ongoing requests and safely shutdown the session.
    FORCEFUL = 2;
}
```

### `Shutdown` リクエスト/レスポンス

```proto
// requests shutdown the session.
message request.Shutdown {

    // the shutdown type.
    ShutdownType type = 1;
}
```

```proto
// response of Shutdown request.
message response.Shutdown {
    // no special message
}
```

* 備考
  * レスポンスは tateyama の core diagnostic のみ受け付けるため、基本的に空
