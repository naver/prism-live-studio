function addCSS (cssText) {
      var style = document.createElement('style'),  //创建一个style元素
        head = document.head || document.getElementsByTagName('head')[0] //获取head元素
      style.type = 'text/css'
      if (style.styleSheet) { //IE
        var func = function () {
          try {
            style.styleSheet.cssText = cssText
          } catch (e) {

          }
        }
        if (style.styleSheet.disabled) {
          setTimeout(func, 10)
        } else {
          func()
        }
      } else { //w3c
        var textNode = document.createTextNode(cssText)
        style.appendChild(textNode)
      }
      head.appendChild(style)
    }

    addCSS(`::-webkit-scrollbar-track-piece {
            border-radius: 0
        }

        ::-webkit-scrollbar {
            width: 10px;
            height: 6px;
            background-color: #272727
        }

        ::-webkit-scrollbar-thumb:vertical {
            width: 6px;
            height: 50px;
            border: 2px solid #272727;
            background-color: #444;
            border-radius: 5px
        }

        .wrap {
			font-size: 14px;
			font-family: 'Malgun Gothic, Dotum, Gulim';
            background: #1e1e1e;
            color: #bababa
        }
		h1{
			font-size: 14px;
		}
		`)
