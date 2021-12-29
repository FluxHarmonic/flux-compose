(define-module (flux graphics)
  #:use-module ((guix licenses) #:prefix license:)
  #:export (show-preview-window))

(define (show-preview-window width height)
  ;; Init SDL, renderer thread
  ;; Show the window
  (init-graphics width height)
  )

(define (render-graphic graphic)
  ;; Transmit instructions to renderer thread
  (display "I would be rendering here...\n"))
