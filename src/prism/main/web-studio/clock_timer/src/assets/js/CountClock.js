import printLog from './printLog'

function getNowTime () {
  return new Date().getTime()
}

const REPAIR_TIME = 5 //5s修复一次计时器
const MAX_REPAIR_ONCE = 500 //每一秒最多修复500ms

export class CountDown {
  name = 'count'
  countTime = 0
  totalLong = 0
  startTime = null
  pauseTime = 0
  isPaused = false
  timer = 0
  repairNum = REPAIR_TIME
  mold = 'down'
  working = false
  processhandle = () => {}
  endHandle = () => {}
  repairHandle = () => {}

  constructor (options) {
    let {
      totalLong = null,
      mold = 'down',
      countTime = 0,
      startTime = null,
      processhandle = () => {},
      repairHandle = () => {},
      endHandle = () => {}
    } = options
    this.mold = mold
    this.countTime = this.totalLong = countTime
    this.endHandle = endHandle
    this.repairHandle = repairHandle
    this.processhandle = (v) => {
      if (Number.isNaN(v) || v < 0) {
        v = 0
      }
      if (v > 359999) {
        v = 359999
        clearTimeout(this.timer)
      }
      processhandle(v)
    }
    this.processhandle(this.countTime)
  }

  start (countTime) {
    countTime && (this.totalLong = countTime)
    this.countTime = this.totalLong
    if (this.mold === 'down' && this.countTime < 1) return
    this.startTime = getNowTime()
    this.process()
    this.working = true
    this.isPaused = false
  }

  pause () {
    clearTimeout(this.timer)
    this.pauseTime = getNowTime()
    this.working = false
    this.isPaused = true
  }

  resume () {
    if (this.pauseTime) {
      this.startTime = this.startTime + (getNowTime() - this.pauseTime)
    }
    this.process()
    this.working = true
    this.isPaused = false
  }

  process () {
    clearTimeout(this.timer)
    this.pauseTimer = null
    let repair = 0
    let forceRepair = 0 // 是否一次性修复多秒
    if (this.countTime % this.repairNum === 1) {
      let offset = getNowTime() - this.startTime
      let bar = this.mold === 'down' ? this.totalLong - this.countTime : this.countTime - this.totalLong
      repair = Math.abs(offset - bar * 1000)
      if (repair > 1000) {
        forceRepair = this.mold === 'down' ? this.totalLong - Math.floor(offset / 1000) : this.totalLong + Math.floor(offset / 1000)
        console.log('force Repair', forceRepair, getNowTime(), this.startTime)
      } else {
        repair = Math.min(MAX_REPAIR_ONCE, repair) // 每次最多修复200ms，避免一秒内修复太多，导致计时器看起来异常
      }
      this.repairHandle(repair)
    }
    this.timer = setTimeout(() => {
      this.countTime = forceRepair ? forceRepair : (this.mold === 'down' ? this.countTime - 1 : this.countTime + 1)
      if (this.mold === 'down' && this.countTime < 1) {
        this.end(true)
      } else {
        this.processhandle(this.countTime)
        this.process()
      }
    }, 1000 - repair)
  }

  end (isNature) {
    clearTimeout(this.timer)
    this.processhandle(0)
    this.endHandle(isNature)
    this.working = false
    this.isPaused = false
  }

  reset () {
    clearTimeout(this.timer)
    this.working = false
    this.countTime = this.totalLong
    this.processhandle(this.countTime)
    this.isPaused = false
  }
}

export class Clock {
  name = 'clock'
  startTime = 0
  timer = 0
  repairNum = REPAIR_TIME
  working = false
  processhandle = () => {}
  endHandle = () => {}
  repairHandle = () => {}

  constructor (options) {
    let {
      processhandle = () => {},
      repairHandle = () => {},
      endHandle = () => {}
    } = options
    this.processhandle = processhandle
    this.endHandle = endHandle
    this.repairHandle = repairHandle
    this.processhandle(new Date())
    this.start()
  }

  start () {
    this.startTime = getNowTime()
    this.process()
    this.working = true
  }

  process () {
    clearTimeout(this.timer)
    this.timer = setTimeout(() => {
      this.processhandle(new Date())
      this.process()
    }, 1000)
  }

  end () {
    clearTimeout(this.timer)
    this.endHandle(this)
    this.working = false
  }

  reset () {}
}
