# IPC 接続における BLOB データ授受に関するデザイン

## この文書について

* Tsurugi の IPC 接続において、クライアントが BLOB を授受する際の過渡期的なデザインを提示し、問題点の発見や作業に必要な工程・工数の見積もりを行う
  * ただし、本デザインは特定の条件を満たした環境のみで動作する「特権モードの IPC 接続」とも呼ぶべきもので、将来的には要件を緩和した任意の接続方式においても BLOB を利用可能にする予定
  * なお、特権モード自体は IPC 接続に限らず、他の接続方式においても条件を満たせば利用可能とする予定
* 本文書は、クライアントとサーバ間の BLOB データの授受に限って記述し、BLOB データの永続化等は他のデザイン文書 ([BLOB データの永続化に関するデザイン](./blob-store-design_ja.md)) に任せる
  * 具体的には、BLOB データをクライアントから各サービスに引き渡す方法と、逆に各サービスからクライアントに BLOB データを引き渡す方法について解説する

## 基本的な考え方

* ローカルファイルシステム上のファイルを介して、クライアント・サーバ間で BLOB データの授受を行う (特権モード)
  * このため、同一のローカルファイルシステムを共有する IPC 環境でのみ動作する
  * また、クライアント・サーバ間で同一のファイルを閲覧できる必要があり、クライアント・サーバそれぞれに一定の特権が必要である
    * Tsurugi プロセスは、クライアントが指定したファイルシステム上のファイルを読み出せる必要がある
    * クライアントプロセスは、 Tsurugi が指定したファイルシステム上のファイルを読み出せる必要がある
* 将来的に任意の接続方式においても BLOB データの授受を行うつもりであるため、クライアント API (Tsubakuro) や、サーバ側のサービス API (Tateyama) はそれを見越したデザインにする
  * 相手のローカルファイルシステム上のファイルパスがそのまま渡るようなデザインにはしない
  * 標準的な接続方式においては、通信路を介して BLOB データそのものをコピーする方式を想定している

## 境界デザイン

### 境界: クライアントの BLOB 送信

方針:
`Session.send()` メソッドに対し、 `payload` のほかに BLOB に関するデータを渡せるようにする。

```java
public interface Session {

    /**
     * Sends a message to the destination server.
     * @param <R> the result value type
     * @param serviceId the destination service ID
     * @param payload the message payload
     * @param blobs the blobs to send
     * @param processor the response processor
     * @return the future of response
     * @throws IOException if I/O error was occurred while requesting
     */
    <R> FutureResponse<R> send(
        int serviceId,
        @Nonnull byte[] payload,
        @Nonnull List<? extends BlobInfo> blobs,
        @Nonnull ResponseProcessor<R> processor) throws IOException;
}
```

`BlobInfo` インターフェースは、BLOB データの情報を表す。

```java
/**
 * An abstract super interface of BLOB data to send to Tsurugi server.
 */
public interface BlobInfo {

    /**
     * Returns the channel name for sending this BLOB data.
     * <p>
     * The channel name is used to identify the BLOB data in the server side,
     * so that it must be unique in the same request.
     * </p>
     * @return the channel name
     */
    String getChannelName();

    /**
     * Returns whether there is a file that represent this BLOB data in the local file system.
     * @return {@code true} if there is a file, otherwise {@code false}
     * @see #getPath()
     */
    boolean isFile();

    /**
     * Returns the path of the file that represents this BLOB data, only if it exists.
     * @return the path of the file, or empty if it does not exist
     * @see #isFile()
     */
    Optional<Path> getPath();
}
```

今回のデザインでは、すべての BLOB データはローカルファイルシステム上に配置することにしているため、以下のようなファイルと関連付けた実装を提供する。

```java
public class FileBlobInfo implements BlobInfo {

    private final String channelName;
    private final Path path;

    /**
     * Creates a new instance.
     * @param channelName the channel name
     * @param path the path of the file that represents this BLOB data
     */
    public FileBlobInfo(@Nonnull String channelName, @Nonnull Path path) {
        Objects.requireNonNull(channelName);
        Objects.requireNonNull(path);
        this.channelName = channelName;
        this.path = path;
    }

    @Override
    public String getChannelName() {
        return channelName;
    }

    @Override
    public boolean isFile() {
        return true;
    }

    @Override
    public Optional<Path> getPath() {
        return Optional.of(path);
    }

    // TODO: toString
}
```

----
Note:

今回の方針では、クライアントとサーバーで同一のファイルシステム上のファイルを利用した BLOB データのやり取りを想定するため、転送路には上記のチャンネル名とファイルパスのペアをリクエストに載せて送ればよさそう。

### 境界: サービス側の BLOB 受信

方針:
`tateyama::api::server::request` クラスで、 `payload` のほかに BLOB データを受け取れるようにする。

```cpp
class request {
public:

    /**
     * @brief returns whether the request has a BLOB data with the specified channel name.
     * @param channel_name the channel name
     * @return true if the request has the BLOB data
     * @return false otherwise
     */
    [[nodiscard]] bool has_blob(std::string_view channel_name) const noexcept = 0;

    /**
     * @brief returns the BLOB data with the specified channel name.
     * @param name the channel name
     * @return the BLOB data
     * @throws if there is no such the BLOB data
     */
    [[nodiscard]] blob_info const& get_blob(std::string_view name) const = 0;
};
```

`blob_info` クラスは、クライアントから送付された BLOB データの情報を提供する。
`tateyama::api::server::request` と強く関連するため、同一の名前空間上に配置し、将来の実装を考慮して pure virtual なインターフェースとしてデザインしておく。

```cpp
/**
 * @brief an abstract super class of BLOB data from the client.
 */
class blob_info {
public:

    // TODO virtual dtor

    /**
     * @brief returns the channel name of the BLOB data.
     * @return the channel name
     */
    [[nodiscard]] virtual std::string_view channel_name() const noexcept = 0;

    /**
     * @brief returns the path of the file that represents the BLOB.
     * @details If the BLOB data is temporary, the file may has been lost.
     * @return the path of the file
     * @see is_temporary()
     */
    [[nodiscard]] virtual std::filesystem::path path() const noexcept = 0;

    /**
     * @brief returns whether the file is temporary, and created in the database process.
     * @details If the file is temporary, the service can remove of modify the file while processing the request.
     * @return true if the BLOB data is temporary
     * @return false otherwise
     */
    [[nodiscard]] virtual bool is_temporary() const noexcept = 0;

    /**
     * @brief disposes temporary resources underlying in this BLOB data.
     * @note If the temporary resources are already disposed, this operation does nothing.
     */
    [[nodiscard]] virtual void dispose() = 0;
};
```

上記は、常に `path()` によってローカルファイルシステム上の BLOB データを表すファイルパスが得られることを前提とする。
これは、将来的にローカルファイルシステム以外で BLOB データの授受を行ったとしても、それを一度ローカルファイルシステム上に保存してから利用することを想定している。

`is_temporary()` は、BLOB データを表すファイル (`path()`) を変更してもよいかどうかを表す。
クライアントが用意したファイルをそのまま参照する場合は `false` でなければならない。
また、 `is_temporary()` が `true` である場合、リクエスト処理の終了時に当該ファイルは削除される。

----
Note:

今回の方針では、クライアントから送られてきたチャンネル名とファイルパスを `blob_info` クラスでカプセル化し、それらを `request` オブジェクト経由で取り出せるようにすればよさそう。

### 境界: サービス側の BLOB 送信

方針:
`tateyama::api::server::response` クラスに、レスポンスに含めるべき BLOB データの一覧を登録できるようにする。

```cpp
class response {
public:
    /**
     * @brief Pass the BLOB data as the response.
     * @pre body_head() and body() function of this object is not yet called
     * @param blob the BLOB data to pass
     * @return status::ok if the BLOB data was successfully registered
     * @return status::not_found if contents is not found in this BLOB data
     * @return status::already_exists if another BLOB data with the same channel name is already added
     */
    virtual status add_blob(std::unique_ptr<blob_info> blob) = 0;
};
```

なお、 複数の BLOB データを非同期的に順次クライアントに送り付けるようなデザインは想定しない。
クライアントは、 `body()` によってレスポンス本体が届いたのちに BLOB データにアクセスできるようになる。

なお、 `blob_info::is_temporary()` が `true` であっても、こちらはクライアントが BLOB データを受け取り終わるまでは当該ファイルの削除は遅延される。
現在はそのようなイベントを検知する手段は存在しないため、ワークアラウンドとしてより上位のスコープをテンポラリの BLOB ライフサイクルと定めて削除する必要がある。

----
Note:

今回の方針では、クライアントとサーバーで同一のファイルシステム上のファイルを利用した BLOB データのやり取りを想定するため、転送路には `blob_info` に含まれるチャンネル名とファイルパスのペアをレスポンスに載せて送ればよさそう。

### 境界: クライアントの BLOB 受信

方針:
`wire.Response` インターフェースの `openSubResponse` 経由で BLOB データの中身を読み出せるようにする。

既に存在しているインターフェースのため、境界に新たな API を生やすことはしない。

----
Note:

今回の方針では、サーバから送られてきたチャンネル名をサブレスポンス名に割り当て、パスに指定された先のファイルを `InputStream` として返せばよい。

----
Note:

今後、性能によっては以下のような機能が必要になるかもしれない:

* BLOB データにランダムアクセスする
* BLOB データを別のファイルにコピーする

上記は必要に応じて `wire.Response` インターフェースを拡張する方針で考えている。

## クライアント API (SQL)

### クライアント: BLOB データの送付

Tsurugi の SQL において BLOB データを指定するには、必ず prepared statement の placeholder を経由して行う。

```java
SqlClient sql = ...;
PreparedStatement ps = sql.prepare(
        "INSERT INTO t VALUES(1, :bd)",
        Placeholders.of("bd", SqlCommon.Blob.class)).await();
Transaction tx = ...;
tx.executeStatement(
        ps,
        Parameters.blobOf("bd", Paths.get("/path/to/object.bin"))).await();
```

現状では、 `common.proto` の `Clob`, `Blob` は空の状態だが、以下のようにファイルパスを渡せるように変更する。

```proto
message common.{Clob, Blob} {

    // the channel name to transfer this large object data.
    string channel_name = 1;

    // the data of this large object.
    oneof data {

        // absolute path of the large object data on the local file system (only for privileged mode)
        string local_path = 2;
    }
}
```

次に、パラメータに `Blob`, `Clob` のいずれかが含まれていた場合、抜き出して `Session.send()` に BLOB として別途渡してやる必要がある。
また、SQL 実行エンジン側では `Blob` オブジェクトに含まれる `data` フィールドを直接は利用せず、 `request.get_blob()` 経由でデータを取り出すことになる。

なお、その時チャンネル名は、 `channel_name` フィールドのものを利用し、名前が衝突した際には `IllegalArgumentException` をスローする。

### クライアント: BLOB 参照を含む結果セットの解析

SQL の結果セットに BLOB/CLOB 型のデータが含まれる場合、 Tsubakuro 上では **BLOB 参照** として受け取ることができる。

BLOB 参照は 8 + 64 bits (または可変長) のデータ列として表現され、以下のような構造を持つ:

```proto
// Represents reference to BLOB data.
message common.LargeObjectReference {
    // The provider type who holds the large object data.
    LargeObjectProvider provider = 1;

    // Object ID of the large object (unique in the provider).
    uint64 object_id = 2;

    oneof contents_opt {
        // large object contents for small data.
        bytes contents = 3;
    }
}

// Represents a kind of BLOB source location.
enum common.LargeObjectProvider {

    // the large object data is in BLOB reference itself.
    LARGE_OBJECT_PROVIDER_UNSET = 0;

    // the large object data is persisted in the log-datastore.
    DATASTORE = 1;

    // the large object data is located in SQL engine temporarily.
    SQL = 2;
}
```

なお、現状では `ResultSet` から BLOB 参照を取り出すメソッドが存在しないため、以下のように追加する。

```java
public interface RelationCursor ... {

    /**
     * Retrieves a {@code BLOB} reference on the column of the cursor position.
     * <p>
     * You can only take once to retrieve the value on the column.
     * </p>
     * @return the reference value
     * @throws IllegalStateException if the value has been already fetched
     * @throws IllegalStateException if this cursor does not point to any columns
     * @throws IOException if I/O error was occurred while extracting the column data
     * @throws IOException if the value type is not matched
     * @throws ServerException if server error was occurred while retrieving this relation
     * @throws InterruptedException if interrupted while extracting the column data
     * @see com.tsurugidb.sql.proto.SqlCommon.AtomType#BLOB
     */
    default BlobReference fetchBlob() throws IOException, ServerException, InterruptedException {
        throw new UnsupportedOperationException();
    }

    /**
     * Retrieves a {@code CLOB} reference on the column of the cursor position.
     * <p>
     * You can only take once to retrieve the value on the column.
     * </p>
     * @return the reference value
     * @throws IllegalStateException if the value has been already fetched
     * @throws IllegalStateException if this cursor does not point to any columns
     * @throws IOException if I/O error was occurred while extracting the column data
     * @throws IOException if the value type is not matched
     * @throws ServerException if server error was occurred while retrieving this relation
     * @throws InterruptedException if interrupted while extracting the column data
     * @see com.tsurugidb.sql.proto.SqlCommon.AtomType#CLOB
     */
    default ClobReference fetchClob() throws IOException, ServerException, InterruptedException {
        throw new UnsupportedOperationException();
    }
}
```

なお、本体コードは `ResultSetImpl` 側に実装することになる。

`BlobReference`, `ClobReference` は、クライアントから変更できないように以下のような空のインターフェースを用意する。

```java
/**
 * An abstract super interface of large object references.
 */
public interface LargeObjectReference {
    // no special methods.
}

/**
 * Represents a reference to BLOB data.
 */
public interface BlobReference extends LargeObjectReference {
    // no special methods.
}

/**
 * Represents a reference to CLOB data.
 */
public interface ClobReference extends LargeObjectReference {
    // no special methods.
}
```

上記を実装したクラスではいずれも Protobuf の `common.BlobReference` を保持し、実際に利用する際にはキャストして取り出す (BLOB, CLOB 共通)。
キャストに失敗した場合、非対応の `BlobReference`, `ClobReference` であるとして `IllegalStateException` をスローする。

----
Note:

`BlobProvider` に複数の種類があるのは、永続化した BLOB データと、一時的に生成された BLOB データを同様に取り扱えるようにするため。
例えば、計算の過程で BLOB データを生成するようなケースや、 dump 処理によって生成したファイルを BLOB として取り出すようなケースでは、 SQL エンジン上に一時ファイルとして生成される。

----
Note:

`RelationCursor` のインターフェースメソッドは `default` 修飾子を付けて定義している。
これは、周辺プロダクトでモック等を作成しているケースが考えられるため。

----
TBD:

BLOB 参照の内部表現は暫定的なもので、 `object_id` 部分はログ・データストア上の実装とそろえること。
[Tsubakuro Result Set Ser/De format](https://github.com/project-tsurugi/jogasaki/blob/master/docs/data-serde-ja.md) では仮に 128bit OID のみを想定していたが、併せて変更が必要になる。

また、小さな BLOB データの最適化を行う場合、 `BLOB_PROVIDER_UNSET` の際にデータ本体を埋め込む

### クライアント: BLOB 参照から BLOB データを取得する

BLOB 参照を取得したクライアントは、 `SqlClient` 経由で BLOB データを取得することができる。

```java
SqlClient sql = ...;
BlobReference ref = ...;

try (InputStream in = sql.openInputStream(ref).await()) {
    // BLOB データを読み込む
}
```

CLOB についても類似の方法でデータを取得できる。

```java
SqlClient sql = ...;
ClobReference ref = ...;

try (Reader reader = sql.openReader(ref).await()) {
    // CLOB データを読み込む
}
```

以下のようなメッセージを想定する (SQL service):

```proto
// request to retrieve large object data.
message request.GetLargeObjectData {
    // the reference to large object data to retrieve.
    common.LargeObjectReference reference = 1;
}

// response of GetLargeObjectData request.
message response.GetLargeObjectData {

    // request is successfully completed.
    message Success {
        // the data channel name of retrieved large object data.
        string channel_name = 1;
    }

    reserved 1 to 10;

    // the response body.
    oneof result {
        // request is successfully completed.
        Success success = 11;

        // error was occurred.
        Error error = 12;
    }
}
```

----
Note:

小さな BLOB を直接送付する最適化を行う場合、 `LargeObjectReference` 内に BLOB/CLOB データが直接含まれているため、サーバへの問い合わせをスキップしてよい。

----
Note:

上記のほかに、ファイルとして受け渡されるBLOB/CLOBの扱いを高速に行うため、以下のクラスを追加する。

```java
/**
 * Represents large object cache.
 */
public interface LargeObjectCache extends AutoCloseable {

    /**
     * Returns the path of the file that represents the large object, only if it exists.
     * The returned Path is available until close() of this object is invoked.
     * @return the path of the file, or empty if it does not exist
     */
    Optional<Path> find();

    /**
     * Copy the large object to the file indicated by the given path.
     * Files created by this method are not affected by close() of this object.
     * @param destination the path of the destination file
     * @throws IOException if I/O error was occurred while copying the large object to the file
     *                     or it encounters a situation where find() returns an empty Optional
     */
    void copyTo(Path destination) throws IOException;

    /**
     * Closes the object cache. The file whose Path has been returned by find() may be deleted.
     */
    @Override
    void close();
}
```

----
TBD:

BLOB データのライフサイクルをより明確にするため、 acquire/release のような仕組みを導入してもよい。
`acquire` 時にハンドルと本体データのストリームを取り出し、本体データの利用終了後に `release` で解放するという方式。
ただし、やりたいことに対してやや大仰にも思えるし、そもそも BLOB の参照を発行したリクエストは別に存在するため、この方式は適切かどうかは検討が必要。

```java
SqlClient sql = ...;
LargeObjectReference ref = ...;
try (LargeObjectData data = sql.acquireLargeObjectData(ref).await()) {
    try (InputStream in = data.openInputStream()) {
        // BLOB データを読み込む
    }
} // LargeObjectData.close() 時に release が呼ばれる
```

なお、上記のようなモデルにすると、特権モード時のアクセスは以下のようにわかりやすいコードを書ける。

```java
SqlClient sql = ...;
LargeObjectReference ref = ...;
try (LargeObjectData data = sql.acquireLargeObjectData(ref).await()) {
    // 読み出し専用のコピーを取得する
    // 特権モードであれば元ファイルを直接読み出せる
    Path path = data.getReadOnlyCopy();

    // BLOB データを読み込む
} // LargeObjectData.close() 時に release が呼ばれる
```

このようなデザインの場合、 `acquire` で BLOB ハンドルを取得し、 `release` で解放する方式になる。

単に一時 BLOB ファイルをクリアしたいだけであれば、 `SqlClient.clearLargeObjects()` のような直接的なメンテナンスメソッドを提供するという手もある。

なお、永続化された BLOB データについては、トランザクションを閉じるまでは GC されないようにすることは比較的容易であるため、この仕組みは必要なさそう。
