#!/afs/cats.ucsc.edu/courses/cse112-wm/usr/racket/bin/mzscheme -qr
;; $Id: sbi.scm,v 1.13 2020-01-10 12:51:12-08 - - $
;;
;; NAME
;;    sbi.scm - silly basic interpreter
;;
;; SYNOPSIS
;;    sbi.scm filename.sbir
;;
;; DESCRIPTION
;;    The file mentioned in argv[1] is read and assumed to be an SBIR
;;    program, which is the executed.  Currently it is only printed.
;;

(define *stdin* (current-input-port))
(define *stdout* (current-output-port))
(define *stderr* (current-error-port))

(define *run-file*
    (let-values
        (((dirpath basepath root?)
            (split-path (find-system-path 'run-file))))
        (path->string basepath))
)

(define (die list)
    (for-each (lambda (item) (display item *stderr*)) list)
    (newline *stderr*)
    (exit 1)
)

(define (usage-exit)
    (die `("Usage: " ,*run-file* " filename"))
)

(define (readlist-from-inputfile filename)
    (let ((inputfile (open-input-file filename)))
         (if (not (input-port? inputfile))
             (die `(,*run-file* ": " ,filename ": open failed"))
             (let ((program (read inputfile)))
                  (close-input-port inputfile)
                         program))))

;;;;;;;;;;; symbol hash and symbol-get ;;;;;;;;;;;;;;;;;
;contains existing symbols in hash and can hold new ones.
(define *symbol-table* (make-hash))
(define (symbol-get key)
        (hash-ref *symbol-table* key))
(define (symbol-put! key value)
        (hash-set! *symbol-table* key value))

(for-each
    (lambda (pair)
            (symbol-put! (car pair) (cadr pair)))
    `(

        (log10_2 0.301029995663981195213738894724493026768189881)
        (sqrt_2  1.414213562373095048801688724209698078569671875)
        (e       2.718281828459045235360287471352662497757247093)
        (pi      3.141592653589793238462643383279502884197169399)
        (div     ,(lambda (x y) (floor (/ x y))))
        (log10   ,(lambda (x) (/ (log x) (log 10.0))))
        (mod     ,(lambda (x y) (- x (* (div x y) y))))
        (quot    ,(lambda (x y) (truncate (/ x y))))
        (rem     ,(lambda (x y) (- x (* (quot x y) y))))
        (+       ,+)
        (^       ,expt)
        (ceil    ,ceiling)
        (exp     ,exp)
        (floor   ,floor)
        (log     ,log)
        (sqrt    ,sqrt)
    (abs     ,abs)
    (acos    ,acos)
    (asin    ,atan)
    (cos     ,cos)
    (round   ,round)
    (sin     ,sin)
    (tan     ,tan)
        (<         ,<)
        (>         ,>)
        (=         ,=)
        (<=       ,<=)
        (>=       ,>=)
        (!=       ,(lambda (x y) (not (equal? x y))))
        (eof      ,0.0)
    
    
     ))
;;;;;;;;;;;;;;;; op hash and eval-expr ;;;;;;;;;;;;;;
;hash table for arithmetic operators
(define fnhash (make-hash))
(for-each
    (lambda (item) (hash-set! fnhash (car item) (cadr item)))
    `((+ ,+)
      (- ,-)
      (* ,*)
      (/ ,/)
      (< ,<)
      (> ,>)
      (= ,=)
      (<= ,<=)
      (>= ,>=)
      (^ ,expt)
      (ceiling    ,ceiling)
        (exp     ,exp)
        (floor   ,floor)
        (log     ,log)
        (sqrt    ,sqrt)
    (abs     ,abs)
    (acos    ,acos)
    (asin    ,atan)
    (cos     ,cos)
    (round   ,round)
    (sin     ,sin)
    (tan     ,tan)
    (atan    ,atan)
))
;performs arithmetic
(define (evalexpr expr)
    (cond ((number? expr) (+ 0.0 expr))
          ((symbol? expr) (evalexpr (symbol-get expr)))
          ((equal? (car expr) 'asub) (vector-ref 
          (symbol-get (cadr expr)) 
          (exact-round (evalexpr (caddr expr)))))
          (else (let ((fn (hash-ref fnhash (car expr)))
                      (args (map evalexpr (cdr expr))))
                     (apply fn args)))))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;; program-hash ;;;;;;;;;;;
;goes through top level list once and adds the 
;labels and their respective addresses in the hash table
;then we call our main function

(define *hash* (make-hash))
(define (program-hash program)
(define *list* program)

(define (show label item)
        (newline)
        (display label) (display ":") (newline)
        (display item) (newline))


  (define (put-in-hash list)
        (when (not (null? list))
              (let ((first (cdar list)))
                   (when (and (not (null? first))
                   (not (pair? (cadar list))))
                         (hash-set! *hash* (cadar list) list)))
        
              (put-in-hash (cdr list))

          ))


(put-in-hash *list*)
(symbol-put! 'flag 0) ;flag used for goto

(interpret-program program)
  
 )
;;;;;;;;;;;;;;;;


;;;;;;;;;;; interpret-proram ;;;;;;;;;;
;acts as our main function
;each time it is called our flag variable is reset
;when the program list is not null
;determine if there is a statement and 
;execute it
;if it was a non-goto statement then 
;call interpret-program recursively with the cdr
;if it was a goto statement then the flag
;will have changed to 1 which means we call interpret-program
;on the value held in the label (the flag
;being changed happens in the interpret-goto function)

(define (interpret-program program)
  (symbol-put! 'flag 0)
   
  (when (not (null? program))
   
    
    (find-stmnt (car program))
    (if (equal? (symbol-get 'flag) 0) (interpret-program 
    (cdr program)) (interpret-program (symbol-get 'goto)))
    
    )

)
;;;;;;;;;

;;;;;; find-stmnt ;;;;;
;intermediate function that checks to see where
;the actual statement resides so we can pass it directly to
;our actual statement parser called findL

(define (find-stmnt line)
    
    (cond  ((and (not (null? (cdr line))) (pair? 
    (cadr line)))  (findL (cadr line)) )
               ((and (not (null? (cdr line))) 
               (not (pair? (cadr line))) (not 
               (null? (cddr line)))) (findL (caddr line)))
               ((and (not (null? (cdr line))) 
               (not (pair? (cadr line))) (null? 
               (cddr line))) (printf ""))
    
        (else printf ""))
    
        
)
;;;;;;;;;

;;;;;;; findL
;checks which kind of statement it is and 
;calls the respective interpret function

(define (findL line)
  
    (cond
        ((equal? (car line) 'print) (interpret-print (cdr line)))
        ((equal? (car line) 'let)   (interpret-let   (cdr line)))
        ((equal? (car line) 'dim)   (interpret-dim   (cdr line)))
                ((equal? (car line) 'goto)  
                (interpret-goto  (cdr line)))
                ((equal? (car line) 'if)    
                (interpret-if    (cdr line)))
                ((equal? (car line) 'input) 
                (interpret-input (cdr line)))
        (else (printf ""))
)

)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;; interpret-print ;;;;;;;;
;used to have a different way of reading prints but it was changed

(define (interpret-print line)

(recurse-print line)

)
;;;;;;;;;;;;;;;

;;;;;; recurse-print ;;;;;;
;newer version of interpret-print function

(define (recurse-print line)
  ;(symbol-put! 'expr-flag 0)
  ;if line is null do nothing. next argument to
  ;be printed is a string or number then print it directly
  ;if it is a symbol that exists in the symbol
  ;table then print the value that the symbol holds
  ;if it is a symbol that exosts on the operator
  ;table then we are looking at an expression
  ;so we call evalexpr
  ;at this point we can assume the only other
  ;type of statement left is an asub so we check that
  
  (cond ((null? line) (printf ""))
        ((number? (car line)) (printf "~a" (+ 0.0 (car line))))
        ((string? (car line)) (printf "~a" (car line)))
        ((and (symbol? (car line)) (hash-has-key? 
        *symbol-table* (car line))) (printf " ~a" 
        (symbol-get (car line))))
        ((and (null? (cdr line)) (hash-has-key? fnhash (caar line))
        (printf "~a" (evalexpr (car line)))))
        ((equal? 'asub (caar line)) (print-asub line))
        (else (printf "")))
  
;if the line was null print a newline
;if the tail is not null recursively call the tail
;if we are at the tail then we are done so we print new line
  
  (cond ((null? line) (printf "~n"))
        ((not (null? (cdr line))) (recurse-print (cdr line)))
        ((null? (cdr line)) (printf "~n"))
        (else printf ""))
 
    
)
;;;;;;;;;;;;;;;

;;;;;;;;;;; print-exp ;;;;;;;
(define (print-exp line)
(printf "~a" (car line))
(cond ((symbol? (cadr line)) (printf "~a" (symbol-get (cadr line))))
    
    (else (printf "~a"  (evalexpr (cadr line))))
)
)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;;;;;;; interpret-let ;;;;;;;;;;;;;;;
;;;;;;; if car is a pair then it must be an asub
;variable otherwise its a single symbol
(define (interpret-let line) 
(if (pair? (car line)) (asub line) 
(symbol-put! (car line) (evalexpr (cadr line))))


) 

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;; print asub;;;;;;;
(define (print-asub line)
(printf "~a" (vector-ref (symbol-get (cadar line)) 
(exact-round (symbol-get (caddar line)))))


)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;;;;;;;intp asub;;;;;;;;;;;
(define (asub line)
 (vector-set! (symbol-get (cadar line)) (exact-round 
 (evalexpr (caddar line))) (evalexpr (cadr line))) 



)
;;;;;;;;;;;;;;

;;;;;;;;;;;;; function never used left in program in case
;something else breaks and i need another way to do asub stuff
(define (asubS line)
;(printf "~a~n" (cadr line))
(cond ((number? (cadr line)) (vector-set! (symbol-get
(cadar line))(exact-round (symbol-get 
(caddar line))) (+ 0.0 (cadr line)) ))
      ((symbol? (cadr line)) (vector-set! (symbol-get
      (cadar line))(exact-round (symbol-get(caddar line)))
      (symbol-get (cadr line)) ))
      
      (else (printf "")))



)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;;;; interpret-dim ;;;;;;;;;;;;

(define (interpret-dim line)
(if (symbol? (cadr (cdar line))) (symbol-put! (car (cdar line))
(make-vector (exact-round (symbol-get (cadr (cdar line))))))
    (symbol-put! (car (cdar line)) (make-vector (cadr (cdar line)))))

)
;;;;;;;;;;;;;;;;;;;;;;

;;;;;;; interpret-goto ;;;;;;;;;
;if a we see a goto statement set the goto flag to 1
;and then add the symbol 'goto to the symbol table,
;where its value is the partial list starting at the label

(define (interpret-goto line)
  
  (symbol-put! 'flag 1)
  (symbol-put! 'goto (hash-ref *hash* (car line)))
 
)
;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;; interpret-if ;;;;;;;;;;;;;;;;
(define (interpret-if line)

(define (call-if-on symbol arg1 arg2)
;(printf "~a " (evalexpr arg1))
;(printf "~a " (evalexpr arg2))
((symbol-get symbol) (evalexpr arg1) (evalexpr arg2))
 )
;if (operator) (argument1) (argument2)
;then goto label otherwise do nothing

(if (call-if-on (caar line) (cadar line)
(caddar line)) (interpret-goto (cdr line)) (printf "")) 

                
) 
;;;;;;;;;;;;;;;;;;;;;;
;;;;;;; input pseudocode
;example: input x

;read the input and set cdr to equal the input.
;example with more vars: input a b c
;read variables recursively until cdr is null
;;;;;;; interpret-input ;;;;;;;;;;

(define (interpret-input line)
 {define (readnumber)
        (let ((object (read)))
             (cond [(eof-object? object) object]
                   [(number? object) (+ object 0.0) ]
                   [else (begin (printf "invalid number: ~a~n" object)
                                (readnumber))] )) }

{define (testinput line)
        (let ((number (readnumber)))
             (cond ((eof-object? number) (symbol-put! 'eof 1.0))
                   ((null? (cdr line)) (symbol-put! (car line) number))
                   ((not (null? (cdr line)))
                   (symbol-put! (car line) number)
                   (testinput (cdr line)))
                   (else (printf "")))
               ) }
(testinput line)

)
;;;;;;;;;;;;;;;;;;;;; stuff already in sbi.scm file ;;;;;;;;;

(define (dump-stdin)
    (let ((token (read)))
         (printf "token=~a~n" token)
         (when (not (eq? token eof)) (dump-stdin))))


;was messing around with main so i made a main2.
;in the end i ended up using main2

(define (main arglist)
    (if (or (null? arglist) (not (null? (cdr arglist))))
        (usage-exit)
        (let* ((sbprogfile (car arglist))
               (program (readlist-from-inputfile sbprogfile)))
              (interpret-program program))))

(define (main2 arglist)
    (if (or (null? arglist) (not (null? (cdr arglist))))
        (usage-exit)
        (let* ((sbprogfile (car arglist))
               (program (readlist-from-inputfile sbprogfile)))
              (program-hash program))))


(if (terminal-port? *stdin*)
    (main2 (vector->list (current-command-line-arguments)))
    ;(main (vector->list (current-command-line-arguments)))
    (printf "sbi.scm: interactive mode~n"))

