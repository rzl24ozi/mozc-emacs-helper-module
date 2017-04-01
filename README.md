# mozc-emacs-helper-module

mozc\_emacs\_helperをemacs-25以降のdynamic moduleにしてみたものです。

Windows10 Pro (mingwおよびcygwin)、Linux (Ubuntu)、FreeBSD (いずれも64bit版)で動作確認しました。

## 使用方法

### mozc-emacs-helper-moduleのビルド

* https://github.com/google/mozc/blob/master/README.md のBuild Instructionsなど参照してmozcをビルドできる環境とmozcソースを準備してください。

  FreeBSDではportsでビルドできます。/usr/ports/japanese/mozc-server/下でmake extractすると/usr/ports/japanese/mozc-server/work/mozc-*/下にソースが展開されます。

* mozcソースsrc/unix/emacs/下に

    mozc-emacs-helper-module.cc  
    emacs-module.h  

  をコピーしてください。emacs-module.hはemacsソースのsrc/下にあるものです。


* mozcソースsrc/unix/emacs/emacs.gypをemacs.gyp.diffのように修正してください。

* 以下のようにターゲットにunix/emacs/emacs.gyp:mozc-emacs-helper-moduleを加え、あとは通常どおりmozcをビルドすればmozc-emacs-helper-moduleができます。

#### Windows

32bit用はsrc/win32/build32/build32.gypをbuild32.gyp.diffのように、64bit用はsrc/win32/build64/build64.gypをbuild64.gyp.diffのように修正してmozcをビルドすればsrc/out\_win/Release/mozc-emacs-helper-module.dll(32bit版)、src/out\_win/Release_x64/mozc-emacs-helper-module.dll(64bit版)ができます。

Google日本語入力と通信するものをビルドする場合はpython build\_mozc.py gyp ...で--branding=GoogleJapaneseInputを指定してください。

なおWindowsでmozcを利用する環境構築については[NTEmacs＠ウィキ](https://www49.atwiki.jp/ntemacs/)が大変参考になりました。まとめられている方に感謝いたします。

#### Linux

python build\_mozc.py gyp --target_platform=Linux 後
python build\_mozc.py build -c Release ...の引数に unix/emacs/emacs.gyp:mozc-emacs-helper-module を指定してビルドすればsrc/out\_linux/Release/lib/libmozc-emacs-helper-module.soができます。

libmozc-emacs-helper-module.soをmozc-emacs-helper-module.soにリネームして使用してください。

#### FreeBSD

/usr/ports/japanese/mozc-server/Makefileのdo-build-mozc-server:のアクションに

        	${BUILD_MOZC_CMD_BULD} unix/emacs/emacs.gyp:mozc-emacs-helper-module

を追加(行頭がタブであることに注意)して、/usr/ports/japanese/mozc-server/下で make を実行しビルドしてください。/usr/ports/japanese/mozc-server/work/mozc-*/src/out\_linux/Release/lib/libmozc-emacs-helper-module.soができます。

libmozc-emacs-helper-module.soをmozc-emacs-helper-module.soにリネームして使用してください。

### 動作確認

* Windows で --branding=GoogleJapaneseInput でビルドした場合はGoogle日本語入力をインストールしておいてください。それ以外は(ビルドした)mozcをインストールしておいてください。

  Linux (Ubuntu)でパッケージのmozc\_serverとのバージョン違いで動かないことがありました。mozc\_serverだけとりあえずビルドしたものに入れ替えると動きました。パッケージと同じバージョンのソースを取得してそれでビルドすればパッケージのmozc\_serverで使えるものができるかと思いますが未確認です。

* 以下の内容のファイルを作成し

    $ emacs -Q -l 作成したファイル名

    としてemacsに読み込ませてください。emacsはdynamic module機能を有効にしてビルドされている必要があります。


        (module-load "mozc-emacs-helper-moduleファイル名(フルパス)")
        (message "%S" (mozc-emacs-helper-module-recv-greeting))
        (message "%S" (mozc-emacs-helper-module-send-sexpr 0 'CreateSession))
        (message "%S" (mozc-emacs-helper-module-send-sexpr 1 'SendKey 1 97))

  [NTEmacs＠ウィキ](https://www49.atwiki.jp/ntemacs/) の [emacs-mozc を動かすための設定（mozc\_emacs\_helper コンパイル編）](https://www49.atwiki.jp/ntemacs/pages/50.html) にあるmozc\_emacs\_helper.exe動作確認の結果と同様な表示が\*Message\*バッファに出力されることを確認してください。

### インストール

* mozc.elをmelpa package から、あるいは手動でload-pathの通っている場所に置くなどしてインストールしておいてください。

* mozc-emacs-helper-moduleとmozc-emacs-helper.elをload-pathの通っている場所に置いてください。

* init.el に (require 'mozc-emacs-helper) とmozcの設定を記述してください。

  mozcの設定はmozc-helperプロセス関連を除き通常と変わりありません。
  既にmozc\_emacs\_helperでmozcを使用しているならinit.elへの(require 'mozc-emacs-helper)の追加だけで使えるかと思いますが、使用しているパッケージにmozc-helperプロセス関連を利用するようなものがもしあれば問題が生じるかもしれません。

## Windows版補足

* Windows版で NTEmacs＠ウィキの[emacs-mozc を動かすための設定（emacs 設定編）](https://www49.atwiki.jp/ntemacs/pages/48.html) にある、セッション接続直後directモードになることへの対処(mozc-session-execute-commandへのadvice)は設定不要です。
mozc-emacs-helper-module.dll内部でCreateSession後IME ONのキーを送っています。

  (参考までに、mozc\_emacs\_heler.cc.diffは同様にCreateSession後IME ONのキーを送るようにmozc\_emacs\_helperを修正するパッチです)


* Windowsの場合mozc-emacs-helper-module.cc先頭でコメントアウトしてある#define disableImeのコメントアウトを外してビルドすると、mozc-emacs-helper-module.dllのロード時にImmDisableIME(-1)が呼び出され、呼び出しプロセスすなわちemacs本体のIMEが無効化されます。この場合

    (global-set-key [auto] 'toggle-input-method)  
    (global-set-key [enlw] 'toggle-input-method)

  で 半角/全角キー にmozcのON/OFFを割り当てたりできるようになります。

  ただしMSDNのImmDisableIMEの説明(ネット検索すると最初の方に出てきます)からするとImmDisableIME呼び出しがロード時に行われるのはまずそうな気がします。利用は自己責任でお願いします。

  gccが使える環境であればemacs-module.hをdisableIme.cと同じところに置いて

    $ gcc -shared -o disableIme.dll disableIme.c -limm32

  でdisableIme.dllを作り、これをロードして大丈夫そうか確認してみてもいいかもしれません。

  disableIme.dllはImmDisableIME(-1)を呼び出して (provide 'disableIme) するだけのモジュールです。

以上
