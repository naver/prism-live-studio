const TYPE = 'webPageLogs'
const key = 'Sidebar Chat'

export default function printLogData (value = '', level = 'INFO', valueKr = null) {
  level = level.toUpperCase()
  let data = {
    module: key,
    field: key,
    level,
    value: typeof value === 'object' ? JSON.stringify(value) : value
  }
  valueKr && (data.valueKr = typeof valueKr === 'object' ? JSON.stringify(valueKr) : valueKr)
  let logData = {
    type: TYPE,
    data: data
  }
  let logDataString = JSON.stringify(logData)
  typeof window.sendToPrism === 'function' && window.sendToPrism(logDataString)
  let logValue = `[${data.level}][${data.value}][${new Date().toString()}]-`
  if (level === 'ERROR') {
    console.error(logValue)
  } else {
    console.log(logValue)
  }
}
