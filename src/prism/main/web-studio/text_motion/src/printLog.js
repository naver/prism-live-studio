const TYPE = 'webPageLogs'
const key = 'Animated Text'

export default function printLogData (value = '', level = 'INFO') {
  level = level.toUpperCase()
  let data = {
    module: key,
    field: key,
    level,
    value: typeof value === 'object' ? JSON.stringify(value) : value
  }
  let logData = {
    type: TYPE,
    data
  }
  let logDataString = JSON.stringify(logData)
  typeof window.sendToPrism === 'function' && window.sendToPrism(logDataString)
  let logValue = `[${data.level}][${data.value}][${new Date().toString()}]`
  console.log(logValue)
}
