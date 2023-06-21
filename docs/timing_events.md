# tateyama endpointタイミングイベントログメッセージ

2023-06-21 horikawa


## 当文書について

tateyamaがログメッセージに出力するsession開始・終了タイミングイベントについて記載する

## タイミングイベント

tateyamaは下記のイベントをverbose log level 35 (tateyama::og_debug_timing_event)で出力する


|  ログプレフィックス  |  イベント内容  | 備考 |
| ---- | ---- | ---- |
| /:tateyama:timing:session:started | セッションを開始した | セッションIDを出力する |
| /:tateyama:timing:session:finished | セッションが終了した | セッションIDを出力する |

出力例
```
I0621 11:41:22.395543 1283106 worker.cpp:25] /:tateyama:timing:session:started 1
...
I0621 11:41:25.408128 1283106 worker.cpp:41] /:tateyama:timing:session:finished 1
```
## タイミングイベント出力方法

タイミングイベントログは [Google glog](https://github.com/google/glog) を使用してイベントログ用の [ロケーションプレフィックス](https://github.com/project-tsurugi/jogasaki/blob/master/docs/logging_policy.md#%E3%83%AD%E3%82%B1%E3%83%BC%E3%82%B7%E3%83%A7%E3%83%B3%E3%83%97%E3%83%AC%E3%83%95%E3%82%A3%E3%83%83%E3%82%AF%E3%82%B9) を指定してサーバーログへ出力される。下記に出力コードの例を示す。
```
    VLOG(log_debug_timing_event) << "/:tateyama:timing:session:started "
        << session_id_;
```

## タイミングイベント検索方法

tateyamaの出力するsession開始・終了タイミングイベントは特定の文字列 `/:tateyama:timing:session` を[ロケーションプレフィックス](https://github.com/project-tsurugi/jogasaki/blob/master/docs/logging_policy.md#%E3%83%AD%E3%82%B1%E3%83%BC%E3%82%B7%E3%83%A7%E3%83%B3%E3%83%97%E3%83%AC%E3%83%95%E3%82%A3%E3%83%83%E3%82%AF%E3%82%B9) の先頭部分に持つ。サーバーログをその文字列で検索することでイベントログの行を抜き出す事ができる。

```
grep -a '/:tateyama:timing:session' <file>
```
(サーバーログがバイナリファイルと誤認識されることがあるので `-a` オプションを付加している)