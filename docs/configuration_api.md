# Tateyama configuration API

2022-04-14 horikawa

## この文書について

Tateyama AP基盤に用意したconfiguration APIの使用方法を記述する

## 前提

- Tateyama AP基盤を使用するモジュールはenvironment（tateyama/include/tateyama/api/environment）にアクセス可能

## 初期化
- configurationの初期化は,、Tateyama AP基盤を使用する全モジュールの初期化より前に行う
- それらのモジュールに渡すenvironmentにconfigurationを設定しておく必要があるため

### 方法
まずenvironmentオブジェクトを作成し、次にconfigurationオブジェクトを作成してenvironmentオブジェクトに設定する
```
    auto env = std::make_shared<tateyama::api::environment>();
    if (auto conf = tateyama::api::configuration::create_configuration("file名"); conf != nullptr) {
        env->configuration(conf);
    } else {
        LOG(ERROR) << "error in create_configuration";
        exit(1);
    }
```
ここで、```file名```は、configuration設定ファイル名の絶対パス

cf. "名前"は文字列であることを示す、std::stringでも良い、以下同様

## 設定値の取得
ateyama AP基盤を使用するモジュールがconfigurationとして設定されたpropertyの値を取得する方法を示す

## 前提
モジュールはenvironmentを参照名envとして受け取っている

## 方法
セクション名を```section名```, property名を```property名```、propertyの型は```type```、取得したpropertyの値を格納する変数を```value```と表記する
```
    auto config = env.configuration()->get_section("section名");
    if (config == nullptr) {
        LOG(ERROR) << "cannot find section名 section in the configuration";
        exit(1);
    }
    auto opt = config->get<type>("property名");
    if (!opt) {
        LOG(ERROR) << "cannot find property名 at the section名 in the configuration";
        exit(1);
    }
    auto value = opt.value();
```
上記は、取得対象sectionやpropertyが存在しない場合はexit(1)させるコードを示したが、状況に応じて適宜変更しても良い
