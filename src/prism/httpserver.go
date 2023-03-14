package main

import "fmt"
import "context"
import "io/ioutil"
import "os"
import "encoding/json"
import "net"
import "net/http"

func main() {
	server := http.Server{}

	http.HandleFunc("/", func(res http.ResponseWriter, req *http.Request) {
	})

	http.HandleFunc("/exit", func(res http.ResponseWriter, req *http.Request) {
		go func() {
			server.Shutdown(context.Background())
		}()

		fmt.Printf("{\"message\": \"exit\"}")
	})

	path := "/oauth2/callback/prism"
	if len(os.Args) == 2 {
		path = os.Args[1]
	}
	http.HandleFunc(path, func(res http.ResponseWriter, req *http.Request) {
		defer func() {
			go func() {
				server.Shutdown(context.Background())
			}()
		}()

		code := req.URL.Query().Get("code")
		if code == "" {
			fmt.Printf("{\"message\": \"code is empty\"}")
			http.ServeFile(res, req, "loginFail.html")

			return
		}

		data, err := json.Marshal(struct {
			Path  string `json:"path"`
			Code  string `json:"code"`
			Scope string `json:"scope"`
		}{
			req.URL.Path,
			code,
			req.URL.Query().Get("scope")})

		if err == nil {
			fmt.Print(string(data))
		} else {
			fmt.Printf("{\"message\": \"%v.\"}", err)
			http.ServeFile(res, req, "loginFail.html")

			return
		}

		http.ServeFile(res, req, "login.html")
	})

	listener, err := net.Listen("tcp", "localhost:0")
	if err != nil {
		fmt.Fprintf(os.Stderr, "{\"message\": \"%v.\"}", err)
		return
	}
	fmt.Println(listener.Addr())

	go func() {
		ioutil.ReadAll(os.Stdin)
		server.Shutdown(context.Background())
	}()

	server.Serve(listener)
}
