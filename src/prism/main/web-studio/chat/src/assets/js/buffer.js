class BufferPool {
  options = {
    pollTime: 5000,
    minInterval: 500,
    maxInterval: 1000,
    maxDelay: 30000,
    maxCountDuringInterval: 3
  }

  bufferPool = []
  timer = null

  constructor(target, options) {
    if (options && typeof options === 'object') {
      this.options = Object.assign(this.options, options)
    }
    this.target = target
    this.calcConstants()
  }

  push(data) {
    if (!data) return
    if (Array.isArray(data)) {
      this.bufferPool.push(...data)
    } else {
      this.bufferPool.push(data)
    }
    if (this.bufferPool.length) {
      this.setTimer()
    }
  }

  pop(count = 1) {
    this.target.push(...this.bufferPool.splice(0, count))
  }

  setTimer() {
    this.clearTimer()
    const { interval, count } = this.calcInterval()
    const func = () => {
      if (!this.bufferPool.length) {
        this.clearTimer()
        return
      }
      this.pop(count)
      return func
    }
    this.timer = setInterval(func(), interval)
  }

  clearTimer() {
    if (this.timer) {
      clearInterval(this.timer)
      this.timer = null
    }
  }

  calcConstants() {
    const { minInterval, pollTime, maxDelay, maxCountDuringInterval } = this.options
    this.maxTimesDuringPollTime = Math.floor(pollTime / minInterval)
    this.maxCountPerPollTime = this.maxTimesDuringPollTime * maxCountDuringInterval
    this.maxPollPerMaxDelay = Math.floor(maxDelay / pollTime)
    this.maxCountPerMaxDelay = this.maxPollPerMaxDelay * this.maxCountPerPollTime
    this.maxTimesDuringMaxDelay = this.maxPollPerMaxDelay * this.maxTimesDuringPollTime
  }

  calcInterval() {
    const length = this.bufferPool.length
    if (!length) return
    const { maxInterval, minInterval, pollTime, maxCountDuringInterval } = this.options

    let count
    let interval

    if (length <= this.maxTimesDuringPollTime) {
      count = 1
      interval = Math.min(Math.floor(pollTime / length), maxInterval)
    } else {
      interval = minInterval
      
      if (length <= this.maxCountPerPollTime) {
        count = Math.ceil(length / this.maxTimesDuringPollTime)
      } else if (length <= this.maxCountPerMaxDelay) {
        count = maxCountDuringInterval
      } else {
        count = Math.ceil(length / this.maxTimesDuringMaxDelay)
      }
    }

    return { interval, count }
  }
}

export default BufferPool
