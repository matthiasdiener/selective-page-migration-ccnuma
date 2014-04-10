; ModuleID = 'fasan_rt.bc'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: uwtable
define zeroext i1 @__fasan_check(i8* %Ary, i64 %Start, i64 %End, i64 %Reuse) #0 {
entry:
  %cmp3 = icmp slt i64 %Start, %End
  br i1 %cmp3, label %for.body, label %for.end

for.body:                                         ; preds = %for.body, %entry
  %i.04 = phi i64 [ %add, %for.body ], [ %Start, %entry ]
  %add.ptr = getelementptr inbounds i8* %Ary, i64 %i.04
  tail call void @__fasan_use(i8* %add.ptr)
  %add = add nsw i64 %i.04, 4
  %cmp = icmp slt i64 %add, %End
  br i1 %cmp, label %for.body, label %for.end

for.end:                                          ; preds = %for.body, %entry
  ret i1 true
}

declare void @__fasan_use(i8*) #1

attributes #0 = { uwtable "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = metadata !{metadata !"clang version 3.4.1 (http://llvm.org/git/clang.git 52484201feeed10e3d62cb7ef33194d879d2b374) (git@bitbucket.org:gnarf/fasan.git 76f4531bf8842d01f9050f31f8735bf8f2c9d778)"}
