# tgctl 実行結果のコンソール表示とexit code

horikawa (2023-08-28)

## この文書について

tgctlの各サブコマンドについて、実行結果とexit codeの関係を規定する。

## コンソール表示
本項のメッセージは標準出力（stdout）に出力する。

### start
* tsurugidbプロセスの起動に成功したことを確認できた -> successfully launched tsurugidb
* tsurugidbプロセスの起動に失敗したことを確認できた -> could not launch tsurugidb
* 起動操作を行ってからtimeout時間が経過しても起動成功を確認できなかった -> failed to confirm tsurugidb launch  within the specified time
* 他のtsurugidbプロセスが実行されていたため、起動操作を行わなかった -> another tsurugidb was already running
  
### shutdown, kill 
* コマンド実行後、tsurugidbプロセスが動作していないことを確認できた -> successfully [killed|shutdown] tsurugidb
* 終了操作を行ってからtimeout時間が経過してもtsurugidbプロセスの動作状態を確認できなかった -> could not check tsurugidb status
* コマンド実行時にはtsurugidbプロセスは実行されていなかった -> no tsurugidb was running
 
## exit code
### start
* tsurugidbプロセスの起動に成功したことを確認できた -> 0
* 起動操作を行ってからtimeout時間が経過しても起動成功を確認できなかった -> non 0
* 他のtsurugidbプロセスが実行されていたため、起動操作を行わなかった -> non 0
  
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
