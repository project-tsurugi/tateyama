# データベースメトリクスに関するデザイン

* 2023-12-20 arakawa (NT)
  * 初版

## この文書について

* この文書では、 Tsurugi インスタンスのメトリクスを提供するためのアーキテクチャについて記す。

## コンセプト

* Tateyama に metrics リソース、 metrics サービスをそれぞれ用意する
  * metrics リソースはメトリクス情報を保持する
  * metrics サービスはメトリクスを要求するコマンドを受け付け、メトリクス情報を提供する
* 各サービスは metrics リソースを知っている状態
  * 逆に、 metrics リソース・サービスは各サービスを知らない状態
  * 未知のサービスのメトリクスを管理できるようにするために、このようにした
* 各サービスは定期的に metrics リソースにメトリクス値を通知する
  * 初期化時にメトリクス項目を登録し、その項目を定期的、または変化のたびに更新する形になる
  * 更新頻度はサービスごとに自由に決めてよい
* メトリクスは最新情報のみを記録する
  * ヒストリは保持しない
  * 各サービスの更新タイミングが異なるため、必ずしも一貫した値にならない

## アーキテクチャ

### 用語

* メトリクス項目
  * Tsurugi のメトリクスに含まれる個々の項目
* メトリクス項目キー
  * 各メトリクス項目に割り当てられた一意のキー文字列
* メトリクス項目値
  * メトリクス項目の値
* metrics リソース
  * メトリクス項目の集合を保持する tateyama 上のコンポーネント (リソース)
* metrics サービス
  * メトリクス項目をクライアントに提供する tateyama 上のコンポーネント (サービス)
* メトリクスプロバイダー
  * metrics リソースにメトリクス項目を提供する Tsurugi 上のコンポーネント (e.g. SQL実行エンジン)
* メトリクス項目スロット
  * メトリクスプロバイダーが各メトリクス項目値を報告するためのスロット
* 集約メトリクスグループ
  * メトリクス項目を集約して新たなメトリクス項目とするグループ
* 集約メトリクスグループキー
  * 集約メトリクスグループに割り当てられた一意のキー文字列

### 基本的な挙動

* metrics リソース・サービスの初期化
  * metrics リソース・サービスを tateyama に登録する
    * metrics リソースは比較的高優先度にし、メトリクスプロバイダーのライフサイクル全体をカバーするようにする
* メトリクスプロバイダーの初期化
  * metrics リソースにアクセスし、各メトリクス項目に対するメトリクス項目スロットを作成する
    * このとき、metrics 項目のキーやその他メタ情報 (タイトル、データ型、初期値、アクセス権限等) を指定する
  * metrics リソースはメトリクス項目スロットを提供したメトリクス項目の一覧を作成する
* メトリクスプロバイダーの稼働時
  * メトリクス項目の変化、または定期的にメトリクス値を収集し、対応するメトリクス項目スロットにその値を報告する
  * metrics リソースは報告されたメトリクス項目値を最新のメトリクス項目値として保持する
* metrics サービスの稼働時
  * クライアントからメトリクス情報の要求があったら、metrics リソースに各メトリクス項目の情報を要求する
  * metrics リソースは、各メトリクス項目のメタ情報と、最新のメトリクス項目値を提供する
  * metrics サービスは、そうして集まったメトリクス項目の集合をメトリクス情報としてクライアントに提供する

### 集約メトリクス

いくつかのメトリクス項目は、単体ではなく特定の単位で集計することで重要な指標となるものもある。
このような項目は、メトリクスプロバイダー側で集約するのではなく、metrics サービス側で集約することになる。

以下は、メトリクス項目を集約するための流れである。

1. metrics リソースにメトリクス項目を登録する際、メタデータとして集約メトリクスグループキーを付与して登録する
2. 上記の集約メトリクスグループキーに対応する集約メトリクスグループを、併せて metrics リソースに登録する
3. metrics サービスがメトリクス情報を提供する際、集約メトリクスグループに対してそのグループキーを付与されたメトリクス項目を集約し、新たなメトリクス項目としてメトリクス情報に含める
   * [API: metrics_aggregation](#api-metrics_aggregation) の章でも解説

なお、集約は合計や平均などの集計計算のみでなく、要素群をリスト化する操作も行える。
例えば、各インデックスのメモリ使用量を個々に独立したメトリクス項目としているものを、集計して「各インデックスのメモリ使用量」という単独のメトリクス項目を構成することができる。
この場合、当該メトリクス項目には、その要素に各インデックスのメモリ使用量の情報を有することになる。

### 複数のコンポーネントを連携させる場合

単独のコンポーネントでメトリクス項目の情報を提供できない場合、複数のコンポーネントが連携する必要がある。

以下は jogasaki と shirakami が連動してインデックスごとのメトリクス項目を登録・更新する流れである。
これは、メトリクス項目値の計算を行う shirakami 単体では、当該インデックスの名称等が不明なため、jogasaki がそれらの値を補完する必要があるためである。

1. jogasaki は shirakami に対し、「インデックス単位のメモリ使用量の変化」をトラックするイベントハンドラを登録し、メトリクス項目スロットを保持する
2. jogasaki はインデックスが追加された際、metrics リソースに当該インデックスに関連するメトリクス項目を追加する
3. shirakami は定期的にインデックスをスキャンして、当該インデックスのメモリ使用量をイベントハンドラに通知する
   * このとき、インデックスを識別する情報は storage ID という識別子であり、スキーマ上の名称などは shirakami 単独では不明
4. jogasaki はイベントハンドラを介して、インデックスのメモリ使用量のデータを受け取る
5. jogasaki はストレージIDを元に、関連するメトリクス項目スロットを検出し、shirakami から通知された情報を当該スロットに書き込む

## API

* デザイン方針
  * メトリクス項目の追加は多少コストがかかる
  * メトリクス項目値の通知は CAS 一発分程度
  * 集約は集約器 ([`metrics_aggregation`](#api-metrics_aggregation)) に任せ、メトリクスプロバイダーは個別の値を登録していく

### metrics リソース

#### API: metrics_metadata

`metrics_metadata` は各メトリクス項目のメタデータで、メトリクス項目を metrics リソースに登録する際に使用する。
また、集約のための要素としてのみ存在する項目を定義することも可能で、その場合は登録したメトリクス項目そのものは最終的なメトリクス情報に含まれない。

```cpp
/**
 * @brief represents metadata of metrics item.
 */
class metrics_metadata {
public:
    /**
     * @brief returns the metrics item key.
     * @returns the metrics item key string
     */
    std::string_view key() const noexcept;

    /**
     * @brief returns the metrics item description.
     * @returns the metrics item description
     */
    std::string_view description() const noexcept;

    /**
     * @brief returns the attributes of this item
     * @returns the attributes of this item
     */
    std::vector<std::tuple<std::string, std::string>> const& attributes() const noexcept;

    /**
     * @brief returns whether to include this item to metrics.
     * @details For example, you can disable this feature to use this item only for aggregation, but do not display.
     * @returns true if this metrics item is visible
     * @returns false otherwise
     */
    bool is_visible() const noexcept;

    /**
     * @brief returns key list of the aggregation group which this item participating.
     * @returns the metrics aggregation group keys
     * @return empty vector if this item is not a member of any groups
     */
    std::vector<std::string> const& group_keys() const noexcept;
};
```

* TODO: 認可と関連付け、許可された認証ユーザだけに利用を限定する

#### API: metrics_aggregation

`metrics_aggregation` は各メトリクス項目をグループ化して集約する一連の機構で、その集約結果を新たなメトリクス項目として利用できる。

```cpp
/**
 * @brief represents aggregation of metrics items.
 */
class metrics_aggregation {
public:
    /**
     * @brief returns the metrics aggregation group key.
     * @returns the metrics aggregation group key string
     */
    std::string_view group_key() const noexcept;

    /**
     * @brief returns the metrics item description.
     * @returns the metrics item description
     */
    std::string_view description() const noexcept;

    /**
     * @brief returns a new aggregator of this aggregation.
     * @returns the aggregation operation type
     */
    virtual std::unique_ptr<metrics_aggregator> create_aggregator() const noexcept = 0;
};
```

```cpp
/**
 * @brief calculates aggregation of metrics items.
 */
class metrics_aggregator {
public:
    /// @brief the aggregation result type
    using result_type = std::variant<double, std::vector<metrics_element>>;

    /**
     * @brief creates a new instance.
     */
    constexpr metrics_aggregator() noexcept = default;

    /**
     * @brief disposes this instance.
     */
    virtual ~metrics_aggregator() = default;

    // NOTE: cannot copy, cannot move

    /**
     * @brief add a metrics item and its value.
     * @attention the specified item must be an aggregation target
     */
    virtual add(metrics_metadata const& metadata, double value) = 0;

    /**
     * @brief aggregates the previously added items.
     * @returns the aggregation result
     */
    virtual result_type aggregate() = 0;
};
```

```cpp
/**
 * @brief represents an element of multi-value metrics item.
 */
class metrics_element {
public:
    /**
     * @brief returns the value of this element.
     * @returns the element value
     */
    double value() const noexcept;

    /**
     * @brief returns the attributes of this item
     * @returns the attributes of this item
     */
    std::vector<std::tuple<std::string, std::string>> const& attributes() const noexcept;
};
```

#### API: metrics_store

`metrics_store` は各メトリクス項目の登録、および最新情報の管理・提供を行うオブジェクトで、 metrics リソースの中核をなす。

```cpp
/**
 * @brief A storage that holds the latest value of individual metrics items.
 */
class metrics_store {
public:
    /**
     * @brief registers a new metrics item.
     * @param metadata metadata of metrics item to register
     * @returns the metrics item slot for the registered one
     * @throws std::runtime_error if another item with the same key is already in this store
     */
    metrics_item_slot register_item(metrics_metadata metadata);
    // NOTE: or std::shared_ptr ?

    /**
     * @brief registers a new aggregation of metrics items.
     * @param aggregation aggregation specification to register
     * @throws std::runtime_error if another aggregation with the same key is already in this store
     */
    void register_aggregation(std::unique_ptr<metrics_aggregation> aggregation);

    /**
     * @brief removes the previously registered metrics item or aggregation.
     * @param key the metrics item key, or group key of metrics aggregation
     * @returns true if successfully the registered element
     * @returns false if no such the element in this store
     */
    bool unregister_element(std::string_view key);

    /**
     * @brief enumerates registered metrics items and their values.
     * @param acceptor the callback that accepts each metrics item and its value
     * @attention this operation may prevent from registering metrics items
     */
    void enumerate_items(std::function<void(metrics_metadata const&, double)> const& acceptor);

    /**
     * @brief enumerates registered metrics items of the specified value type.
     * @param acceptor the callback that accepts each metrics aggregation
     * @attention this operation may prevent from registering metrics aggregations
     */
    void enumerate_aggregations(std::function<void(metrics_aggregation const&)> const& acceptor) const;
};
```

```cpp
/**
 * @brief the metrics item slot to accept metrics item values.
 */
class metrics_item_slot {
public:
    /**
     * @brief notifies the latest value of metrics item.
     * @param value the latest value
     */
    void set(double value);

    /// @copydoc set()
    metrics_item_slot& operator=(double value);

    // NOTE: can not copy, can move
};
```

### Protocol Buffers

metrics サービスが利用するであろう Protocol Buffers の定義。

フィールド番号は省略。

```proto
// the metrics value.
message MetricsValue {
    // variant of metrics value, or empty if it is not set.
    oneof value_or_array {
        // just a value.
        double value;

        // sub-elements.
        MetricsElementArray array;
    }
}

// a variation of metrics value that holds multiple elements.
message MetricsElementArray {

    // the metrics elements.
    repeated MetricsElement elements;
}

// an element of MetricsElementArray.
message MetricsElement {

    // the metrics element value.
    double value;

    // attributes of this metrics element.
    map<string, string> attributes;
}

// represents an item of metrics.
message MetricsItem {

    // the metrics item key.
    string key;

    // human-readable description of the item.
    string description;

    // the value of this item.
    MetricsValue value;
}

// represents metrics information.
message MetricsInformation {

    // the individual metrics items.
    repeated MetricsItem items;
}
```

* 備考
  * `tgctl dbstats {show,list}` で使用するメッセージは共通
    * `show` サブコマンドではメトリクス項目値を計算し、各 `MetricsValue` に適切な値を設定する
    * `list` サブコマンドではメトリクス項目値の計算を行わず、 `MetricsValue` の値をすべて空にする
    * `MetricsItem.description` はどちらも常に設定する
