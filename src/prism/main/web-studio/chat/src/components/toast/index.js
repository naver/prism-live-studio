import Vue from 'vue'
import ToastComponent from './Index.vue'

let instance
let PopConstructor = Vue.extend(ToastComponent)

let initInstance = () => {
  instance = new PopConstructor({
    el: document.createElement('div')
  })
  document.body.appendChild(instance.$el)
}

let Toast = (content, options = {}) => {
  if (!content) {
    return
  }
  options.content = content
  initInstance()
  merge(instance.$data, options)
  instance.show = true
}

function merge(target, origin) {
  if (!origin) return
  for (let item in origin) {
    if (origin.hasOwnProperty(item)) {
      let value = origin[item]
      if (value !== undefined) {
        target[item] = value
      }
    }
  }
  return target
}

export default Toast
