# 性能測定・分析時の推奨構成

tsurugiの性能測定/分析時に推奨されるOSの構成を記述する

## numaバランシングの停止

NUMA構成のCPU上で稼働する際にLinuxカーネルがNUMAノード間でページを移動させてバランシングを行う機能がある。データの量と頻度によってはtsurugiのSQL処理においてオーバーヘッドが高くなることがあるため、 パラメーター `/proc/sys/kernel/numa_balancing` を `0` に設定してこの機能を停止することが推奨される。

参考：https://github.com/HewlettPackard/LinuxKI/wiki/NUMA-Balancing

## カーネルパフォーマンスモニタリングの制限を削減

性能分析時にカーネルのパフォーマンスイベントの情報を取得可能にするため、下記のパラメーターを設定することが推奨される。

|                  パラメーター名 | 値                   | 備考                                                                                                                                                           |
|-------------------------:| :---: |:--------------------|
| `/proc/sys/kernel/perf_event_paranoid`  | `-1`  | パフォーマンスイベント情報の取得を制限しない|
| `/proc/sys/kernel/kptr_restrict` | `0` | カーネルアドレスの公開を制限しない| 

## CPUスケーリングの抑制

Linuxデフォルトの設定ではCPUの負荷に応じてCPU周波数が変化する。性能測定時にはこの影響を可能な限り避けるために、下記コマンドによってCPUスケーリングを抑制することが推奨される。

```
cpupower frequency-set --governor performance
```


