# トランスポートバイナリ仕様

2022-05-10 kurosawa

## 本文書について

エンドポイントとサービスで交換されるバイト列のフォーマットについて記述する。具体的には下記のフォーマットについて記述する

- tateyama::api::endpoint::request::payload()でサーバーエンドポイントからサービス側へ送信するバイト列(以下リクエストバイナリという)

- tateyama::api::endpoint::response::body()でサービスからサーバーエンドポイントが受信するバイト列(以下レスポンスバイナリという)

## 仕様

- リクエストバイナリ・レスポンスバイナリはprotobufの公式ライブラリがサポートするdelimited 形式のバイト列とする
  - 複数のメッセージを1つのバイト列で送受信することができる
  - delimited形式については公式文書で詳細な記述がないが、下記のAPIを参照：
    - Java API: [writeDelimitedTo], [parseDelimitedFrom]
    - C++ API: [delimited_message_util.h]
(libprotobuf 3.3以前だと実装されていないので tateyama/include/tateyama/utils/protobuf_utils.h
に必要な関数を実装予定)

[writeDelimitedTo]: https://developers.google.com/protocol-buffers/docs/reference/java/com/google/protobuf/MessageLite#writeDelimitedTo-java.io.OutputStream-

[parseDelimitedFrom]: https://developers.google.com/protocol-buffers/docs/reference/java/com/google/protobuf/Parser#parseDelimitedFrom-java.io.InputStream-

[delimited_message_util.h]: https://github.com/protocolbuffers/protobuf/blob/main/src/google/protobuf/util/delimited_message_util.cc

- リクエストバイナリ・レスポンスバイナリは2個のメッセージからなるdelimitedメッセージであり、1番目にヘッダを、2番目をペイロード用に使用する
  - ヘッダ部分については下記セクションを参照
  - ペイロード部分はクライアントとサービスのみが知識を持っており、この文書の範囲外

### ヘッダ

- リクエストバイナリのヘッダはtateyama/proto/framework/requestで定義されるHeaderメッセージ
https://github.com/project-tsurugi/tateyama/blob/9c100477a1df14b295ae00308e289d2e9a0324ec/src/tateyama/proto/framework/request.proto

- レスポンスバイナリのヘッダはtateyama/proto/framework/responseで定義されるHeaderメッセージ
https://github.com/project-tsurugi/tateyama/blob/9c100477a1df14b295ae00308e289d2e9a0324ec/src/tateyama/proto/framework/response.proto
