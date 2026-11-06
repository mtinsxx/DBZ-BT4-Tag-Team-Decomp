; Explicit edge cases not reliably retained by a C frontend. Resource-free.
@phi_value = external global i32
declare void @abort()

; The unchecked edge must not execute checked pointer encoding of a foreign
; libc function. Hoisting to entry would abort even when choose == 0.
define i32 @guest_phi_checked_edge(i32 %choose) {
entry:
  %take = icmp ne i32 %choose, 0
  br i1 %take, label %merge, label %right
right:
  br label %merge
merge:
  %value = phi i32 [ ptrtoint (ptr @abort to i32), %entry ], [ 0, %right ]
  ret i32 %value
}

define i32 @guest_phi_edge(i32 %choose) {
entry:
  %take = icmp ne i32 %choose, 0
  br i1 %take, label %merge, label %right
right:
  br label %merge
merge:
  %value = phi i32 [ ptrtoint (ptr @phi_value to i32), %entry ], [ 0, %right ]
  ret i32 %value
}

define i32 @guest_phi_switch(i32 %choose) {
entry:
  switch i32 %choose, label %right [ i32 1, label %merge
                                    i32 2, label %merge ]
right:
  br label %merge
merge:
  %value = phi i32 [ ptrtoint (ptr @phi_value to i32), %entry ],
                   [ ptrtoint (ptr @phi_value to i32), %entry ], [ 0, %right ]
  ret i32 %value
}
