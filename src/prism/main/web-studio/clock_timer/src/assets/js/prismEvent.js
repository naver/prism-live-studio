export default {
  // 计时开始
  start (data) {
    send('start', data)
  },
  // 计时自然结束
  finish (data) {
    send('finish', data)
  },
  // 计时暂停
  pause (data) {
    send('pause', data)
  },
  // 计时停止
  stop (data) {
    send('stop', data)
  },
  restart (data) {
    send('restart', data)
  },
  resume (data) {
    send('resume', data)
  },
  cancel (data) {
    send('cancel', data)
  },
  countTimeChangeWhenWorking (data) {
    send('countTimeChangeWhenWorking', data)
  },
  liveStartTimeChangeWhenWorking (data) {
    send('liveStartTimeChangeWhenWorking', data)
  },
  startMusic (data) {
    send('startMusic', data)
  },
  stopMusic (data) {
    send('stopMusic', data)
  },
  pageLoaded (data) {
    send('pageLoaded', data)
  },
  handleFirstOption (data) {
    send('handleFirstOption', data)
  },
}

function send (eventName, data = {}) {
  console.log(eventName, data)
  window.sendToPrism && window.sendToPrism(JSON.stringify({
    module: 'timerClockWidget',
    data: {
      eventName: eventName,
      ...data
    }
  }))
}
