import Vue from 'vue'
import PopComponent from './Index.vue'

let instance
let PopConstructor = Vue.extend(PopComponent)

let initInstance = () => {
  instance = new PopConstructor({
    el: document.createElement('div')
  })
  document.body.appendChild(instance.$el)
}

let Pop = (options = {}) => {
  initInstance()
  merge(instance.$data, options)
  return new Promise((resolve, reject) => {
    instance.show = true
    let handleConfirm = instance.handleConfirm
    let handleCancel = instance.handleCancel
    instance.handleConfirm = () => {
      handleConfirm()
      resolve()
    }
    instance.handleCancel = () => {
      handleCancel()
      if (options.type && options.type === 'alert') {
        resolve()
      } else {
        reject()
      }
    }
  })
}

Pop.confirm = (options = {}) => {
  return Pop(merge({
    type: 'confirm'
  }, options))
}

Pop.alert = (options = {}) => {
  if (instance && instance.show) {
    return new Promise((reslove, reject) => {
      reslove()
    })
  }
  return Pop(merge({
    type: 'alert'
  }, options))
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

export default Pop
