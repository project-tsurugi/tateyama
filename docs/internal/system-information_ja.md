# システム情報取得機能に関するデザイン

## この文書について
* Tsubakuro経由でtsurugidbのシステム情報を取得する機能を設計する

## 基本的な考え方
  * TsubakuroにSystemClientを作る、これが system_information というメッセージをやりとりする
  * tateyamaにsystem-serviceを作り、system_information メッセージをやりとりする
  
## 境界デザイン

### メッセージ
tateyama/proto/systemに下記メッセージを配置する。

```proto
message system.request.Request {
    // service message version (major)
    uint64 service_message_version_major = 1;

    // service message version (minor)
    uint64 service_message_version_minor = 2;

    // reserved for system use
    reserved 3 to 10;

    // the request command.
    oneof command {
        // get system information.
        GetSystemInfo getSystemInfo = 11;
    }
    reserved 12 to 99;
}

// request to get system information.
message system.request.GetSystemInfo {
    // no special properties.
}


// an error occurred.
message system.response.Error {
    // the error message.
    string message = 1;

    // error code
    diagnostic.ErrorCode code = 2;

    // supplemental text for debug purpose
    string supplemental_text = 3;
}

// response of get system information request.
message system.response.GetSystemInfo {
    reserved 1 to 10;

    // the response body.
    oneof result {
        // request is successfully completed.
        Success success = 11;

        // error occurred.
        Error error = 12;
    }

    // request is successfully completed.
    message Success {
        // the system information.
        SystemInfo system_info = 1;
    }
}

// system information.
message system.response.SystemInfo {

    // name field of tsurugi-info.json
    string name = 1;

    // version field of tsurugi-info.json
    string version = 2;

    // date field of tsurugi-info.json
    string date = 3;

    // url field of tsurugi-info.json
    string url = 4;
}

// the error code.
enum system.diagnostic.ErrorCode {
    // the error code is not set.
    ERROR_CODE_NOT_SPECIFIED = 0;

    // the unknown error occurred.
    UNKNOWN = 1;

    // the target is not found.
    NOT_FOUND = 2;
}
```


### クライアント
com.tsurugidb.tsubakuro.systemに下記APIを配置する。

```java
/**
 * A System client.
 * @see #attach(Session)
 */
@ServiceMessageVersion(
        service = SystemClient.SERVICE_SYMBOLIC_ID,
        major = SystemClient.SERVICE_MESSAGE_VERSION_MAJOR,
        minor = SystemClient.SERVICE_MESSAGE_VERSION_MINOR)
public interface SystemClient extends ServerResource, ServiceClient {

    /**
     * The symbolic ID of the destination service.
    */
    String SERVICE_SYMBOLIC_ID = "system";

    /**
     * The major service message version which this client requests.
     */
    int SERVICE_MESSAGE_VERSION_MAJOR = 0;

    /**
     * The minor service message version which this client requests.
     */
    int SERVICE_MESSAGE_VERSION_MINOR = 1;

    /**
     * Attaches to the System service in the current session.
     * @param session the current session
     * @return the System service client
     */
    static SystemClient attach(@Nonnull Session session) {
        Objects.requireNonNull(session);
        return SystemClientImpl.attach(session);
    }

    /**
     * Get the system information of the database to which the session is connected.
     * @return a future of SystemResponse.SystemInfo message
     * @throws IOException if I/O error occurred while sending request
     */
    default FutureResponse<SystemResponse.SystemInfo> getSystemInfo() throws IOException;
}
```

また、`META-INF/tsurugidb/clients`に`SystemClient`を登録する。

### サーバ
#### system service
tateyama/systemにsystem serviceを作る。component idは下記とする。
```
constexpr inline component::id_type service_id_system = 12;
```

memo: system serviceをtsurugidbに組み込む操作はtateyamaで行い、tateyamaの外部（tateyama-bootstrap等）は関与しない。また、システム情報の取得はtsurugidbの起動時に行う。このため、tsurugidbの動作中にtsurugi-info.json（次項）を書き換えても、それはgetSystemInfo()の結果に反映されない。

#### システム情報の取得
* システム情報は、下記フィールドを有するjsonファイル（ファイル名：`tsurugi-info.json`）から取得する。
  
| フィールド名 | 型    | 値 |
|------------:| :---: |:--------------------|
| name        | 文字列 | データベースの製品名 |
| version     | 文字列 | 製品のバージョン     |
| date        | 文字列 | 製品のビルド日時（ISO 8601形式）     |
| uri         | 文字列 | バージョンに対応するgitリポジトリのURL<BR>起点は`https://github.com/project-tsurugi/tsurugidb` |

  * `tsurugi-info.json`の内容例は下記。
```
{
    "name": "tsurugidb",
    "version": "1.8.0-SNAPSHOT-202512100915-18c8535",
    "date": "2025-12-10T09:18Z",
    "url": "//tree/18c853557bfb0a81e14f1181136b060891794cf4"
}
```
memo: `Message SystemResponse.SystemInfo`は、`tsurugi-info.json`の現状に合わせ、name, version, date, urlの文字列を持つMessageとする。将来`tsurugi-info.json`のフィールドが追加された際は、それに応じて`Message SystemResponse.SystemInfo`のフィールドを追加しても良い。

* システム情報を取得する`tsurugi-info.json`の位置は下記とする。
  * 環境変数`TSURUGI_HOME`が設定されている場合は `${TSURUGI_HOME}/lib/tsurugi-info.json`とする。そのファイルが存在しない場合、getSystemInfoリクエストに対して`response.GetSystemInfo.error`の`code`に`diagnostic.ErrorCode.NOT_FOUND`を設定したメッセージを返す。
  * 環境変数`TSURUGI_HOME`が設定されていない場合はtsurugidbの実行ファイル（`libexec/libexec/tsurugidb`）が置かれているディレクトリを起点とする相対パス `../lib/tsurugi-info.json` とする。そのファイルが存在しない場合、getSystemInfoリクエストに対して前項記載と同じエラーメッセージを返す。