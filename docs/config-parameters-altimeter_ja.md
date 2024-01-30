# 構成ファイルのパラメーター（altimeter関連セクション）

tsurugi構成ファイルのaltimeter関連セクションは下記のようなiniファイル形式でコンフィグパラメーターを設定する事ができる。

```
[section]
parameter=value
```

altimeter関連セクションには、event_logセクションとaudit_logがある。両者でサポートされるパラメーター（共通）の一覧を列挙する。

## altimeter関連セクション
セクション名
  - event_log

対象コンポーネント

  - altimeter（event log）

|            パラメーター名 | 型 | 値                            | 備考                   |
|-------------------------:| :---: |:------------------------------|------------------------|
| output  | bool | ログ出力を行うか否か。デフォルトはtrue。 |
| directory | 文字列 | ログファイルを作成するディレクトリ。デフォルトは/var/log/altimeter/event。 |
| file_number | 数値 | ログファイル数。デフォルトは10。 |
| sync | bool | 同期書き込みを行うか否か。デフォルトはfalse。 |
| buffer_size | 整数 | ログバッファのサイズ。デフォルトは1048576。 |
| flush_interval | 整数 | ログをflushする間隔。デフォルトは10000。 |
| flush_file_size | 整数 | ログをflushするサイズ。デフォルトは1048576。 |
| max_file_size | 整数 | ログファイルの最大値。デフォルトは1073741824。 |

セクション名
  - audit_log

対象コンポーネント

  - altimeter（audit log）

パラメータ名、型、値は、directoryのデフォルト値（/var/log/altimeter/audit）を以外はevent_logセクションと同じ。