# tgctl 実行結果のコンソール表示とexit code

horikawa (2023-08-28)

## この文書について

tgctlの各サブコマンドについて、実行結果とexit codeの関係を規定する。

## コンソール表示
本項のメッセージは標準出力（stdout）に出力する。

### start
* tsurugidbプロセスの起動に成功したことを確認できた -> successfully launched tsurugidb
* tsurugidbプロセスの起動に失敗したことを確認できた -> could not launch tsurugidb
* 起動操作を行ってからtimeout時間が経過しても起動成功を確認できなかった -> failed to confirm tsurugidb launch within the specified time
* 他のtsurugidbプロセスが実行されていたため、起動操作を行わなかった -> another tsurugidb was already running
  
### shutdown, kill 
* コマンド実行後、tsurugidbプロセスが動作していないことを確認できた -> successfully [killed|shutdown] tsurugidb
* コマンド実行後もtsubugidbプロセスが動作したままになっていることを確認できた -> could not [kill|shutdown] tsurugidb
* 終了操作を行ってからtimeout時間が経過してもtsurugidbプロセスの終了を確認できなかった -> failed to confirm tsurugidb termination within the specified time
* コマンド実行時にはtsurugidbプロセスは実行されていなかった -> no tsurugidb was running
 
## exit code
### start
* tsurugidbプロセスの起動に成功したことを確認できた -> 0
* 起動操作を行ってからtimeout時間が経過しても起動成功を確認できなかった -> non 0
* 他のtsurugidbプロセスが実行されていたため、起動操作を行わなかった -> 0
  
### shutdown, kill 
* コマンド実行後、tsurugidbプロセスが動作していないことを確認できた -> 0
* 終了操作を行ってからtimeout時間が経過してもtsurugidbプロセスの動作状態を確認できなかった -> non 0
* コマンド実行時にはtsurugidbプロセスは実行されていなかった -> 0

### status
* tsurugidbプロセスの動作状態を確認できた -> 0
* tsurugidbプロセスの動作状態を確認できなかった -> non 0

### その他（datastore操作）
* 指示された操作が成功した -> 0
* 指示された操作が失敗した -> non 0


## tgctl [start|shutdown|kill] の 全体像
tgctlコマンドの正常時における挙動をまとめる。ここで、「正常時」とは、tsurugidbプロセスの動作状態をtimeout時間以内で確認できる状態とする。

### tgsql start
| 実行前の<br>tsurugidb状態 | 実行後の<br>tsurugidb状態 | console message | exit code |
| ---- | ---- | ---- | ---- |
| 動作していない | 動作している | successfully launched tsurugidb | 0 |
| 動作していない | 動作していない | could not launch tsurugidb, as (理由) | non 0 |
| 動作していない | 動作していることが<br>確認できない | failed to confirm tsurugidb launch within the specified time, as (理由) | non 0 |
| 動作している | 動作している | could not launch tsurugidb, as another tsurugidb is already running | 0 |

### shutdown, kill
| 実行前の<br>tsurugidb状態 | 実行後の<br>tsurugidb状態 | console message | exit code |
| ---- | ---- | ---- | ---- |
| 動作している | 動作していない | successfully [shutdown\|killed] tsurugidb | 0 |
| 動作している | 動作している | could not [shutdown\|killed] tsurugidb, as (理由) | non 0 |
| 動作していない | 動作していない | [shutdown\|kill] was not performed, as no tsurugidb was running | 0 |

なお、--monitorオプションで指定するtgctl実行結果ファイル（JSON形式）については、exit codeが0のものはsuccess、non 0のものはfailureを格納する。

また、tsurugidbプロセスの動作状態をtimeout時間以内で確認できない場合は、状態確認できない旨をconsole messageとして表示し、exit codeはnon 0とする。
