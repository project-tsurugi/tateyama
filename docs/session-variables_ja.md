# セッション変数

セッション変数 (session variables) は、各機能の設定をセッションごとに調整するための機能で、以下のコマンド ([`session set`]) で設定できる。

```sh
tgctl session set <session-ref> <variable-name> [<variable-value>]
```

[`session set`]: https://github.com/project-tsurugi/tateyama/blob/master/docs/session-control-cli_ja.md#session-set

## SQL実行エンジン

| 変数名 | データ型 | 初期値 | 説明 |
|:-------|:--------|:--------|:-----|
| `sql.plan_recording` | `bool` | _unset_ | SQL実行計画を記録[^1]するかどうか。未設定 (_unset_) の場合にはデフォルト設定[^2]を利用。 |

[^1]: SQL実行計画の記録には、`EXPLAIN`コマンドを使用する。また、記録先は Altimeter となっているため、同機能が有効な場合のみ利用可能。
[^2]: 未指定の場合、構成定義ファイル内等で設定された値を利用する
