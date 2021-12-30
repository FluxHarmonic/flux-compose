;; Display the loading string
(display "Starting Flux Compose...\n\n")

;; Add our modules to the load path
;; TODO: Make this more robust
(add-to-load-path (canonicalize-path "src/modules/"))
