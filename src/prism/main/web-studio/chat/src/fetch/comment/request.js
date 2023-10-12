import axios from 'axios'
import axiosRetry from 'axios-retry'

const service = axios.create({
  crossDomain: true,
  withCredentials: true, // send cookies when cross-domain requests
  timeout: 5000 // request timeout
})

axiosRetry(service, {retries: 2, retryDelay: () => 2000})

service.interceptors.request.use(
  config => {
    return config
  },
  error => {
    console.log(error) // for debug
    return Promise.reject(error)
  }
)

// response interceptor
service.interceptors.response.use(
  response => {
    return response.data
  },
  err => {
    throw err
  }
)

export default service
