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

    addCSS(`tp-yt-iron-dropdown #contentWrapper ytd-menu-popup-renderer tp-yt-paper-listbox ytd-menu-navigation-item-renderer:last-child{display:none}`)