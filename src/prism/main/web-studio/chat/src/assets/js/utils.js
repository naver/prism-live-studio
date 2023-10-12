export function handleJson (data) {
  if (typeof data === 'string') {
    data = JSON.parse(data)
  }
  return data
}

export function logData (...args) {
  let html = document.getElementById('logArea')
  if (!html) return
  let text = ''
  args.forEach(item => {
    text += ' ' + (typeof item === 'object' ? JSON.stringify(item) : item)
  })
  html.innerHTML = html.innerHTML + '\n' + text
}
