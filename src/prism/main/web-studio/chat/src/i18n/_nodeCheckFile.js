var fs = require('fs')
let en = require('./en.json')
let ko = require('./ko.json')
let id = require('./id.json')
let pt = require('./pt.json')

let langObj = {en, ko, id, pt}
let Title = 'ChatTab'
let keys = []
let langResult = {}
for (let key in langObj) {
  langResult[key] = analyse(langObj[key])
}
keys = [...new Set(keys)]

checkAndWrite()

function analyse (obj) {
  let result = {}
  let values = []
  getOut(obj, result, Title)
  return values

  function getOut (value, result, key = 'title') {
    if (typeof value === 'string' || Array.isArray(value)) {
      let v2 = Array.isArray(value) ? JSON.stringify(value) : value
      keys.push(key)
      values.push(v2)
      result[key] = v2
    } else {
      for (let key2 in value) {
        getOut(value[key2], result, key + '.' + key2)
      }
    }
  }
}

function checkAndWrite () {
  let re = Object.values(langResult).filter(e => {
    return e.length === keys.length
  })
  if (re.length !== Object.keys(langResult).length) {
    console.log('error!: lengthError')
  } else {
    console.log('check length successed\n')
  }

  let text = JSON.stringify({
    keys, ...langResult
  })

  fs.writeFile('./_nodeCheckOut.Json', text, {'flag': 'w'}, function (err) {
    if (err) {
      throw err
    }
    console.log('file write successed')
  })
}

