; ModuleID = 'book-test.ll'
source_filename = "../../../../alex/basic/book-test.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.Book = type { [50 x i8], [50 x i8], [100 x i8], i32 }

@.str = private unnamed_addr constant [14 x i8] c"C Programming\00", align 1
@.str.1 = private unnamed_addr constant [9 x i8] c"Nuha Ali\00", align 1
@.str.2 = private unnamed_addr constant [23 x i8] c"C Programming Tutorial\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i8*, align 8
  %4 = alloca i64, align 8
  %5 = alloca i32, align 4
  %6 = alloca %struct.Book*, align 8
  %7 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 1000000, i32* %2, align 4
  %8 = load i32, i32* %2, align 4
  %9 = zext i32 %8 to i64
  %10 = call i8* @llvm.stacksave()
  store i8* %10, i8** %3, align 8
  %11 = alloca %struct.Book*, i64 %9, align 16
  store i64 %9, i64* %4, align 8
  store i32 0, i32* %5, align 4
  br label %12

12:                                               ; preds = %40, %0
  %13 = load i32, i32* %5, align 4
  %14 = load i32, i32* %2, align 4
  %15 = icmp slt i32 %13, %14
  br i1 %15, label %16, label %43

16:                                               ; preds = %12
  %17 = call i8* @jemalloc(i64 204)
  %18 = bitcast i8* %17 to %struct.Book*
  store %struct.Book* %18, %struct.Book** %6, align 8
  %19 = load %struct.Book*, %struct.Book** %6, align 8
  %20 = getelementptr inbounds %struct.Book, %struct.Book* %19, i32 0, i32 0
  %21 = getelementptr inbounds [50 x i8], [50 x i8]* %20, i64 0, i64 0
  %22 = getelementptr [14 x i8], [14 x i8]* @.str, i64 0, i64 0
  %23 = call i8* @strcpy(i8* %21, i8* %22) #1
  %24 = load %struct.Book*, %struct.Book** %6, align 8
  %25 = getelementptr inbounds %struct.Book, %struct.Book* %24, i32 0, i32 1
  %26 = getelementptr inbounds [50 x i8], [50 x i8]* %25, i64 0, i64 0
  %27 = getelementptr [9 x i8], [9 x i8]* @.str.1, i64 0, i64 0
  %28 = call i8* @strcpy(i8* %26, i8* %27) #1
  %29 = load %struct.Book*, %struct.Book** %6, align 8
  %30 = getelementptr inbounds %struct.Book, %struct.Book* %29, i32 0, i32 2
  %31 = getelementptr inbounds [100 x i8], [100 x i8]* %30, i64 0, i64 0
  %32 = getelementptr [23 x i8], [23 x i8]* @.str.2, i64 0, i64 0
  %33 = call i8* @strcpy(i8* %31, i8* %32) #1
  %34 = load %struct.Book*, %struct.Book** %6, align 8
  %35 = getelementptr inbounds %struct.Book, %struct.Book* %34, i32 0, i32 3
  store i32 6495407, i32* %35, align 4
  %36 = load %struct.Book*, %struct.Book** %6, align 8
  %37 = load i32, i32* %5, align 4
  %38 = sext i32 %37 to i64
  %39 = getelementptr inbounds %struct.Book*, %struct.Book** %11, i64 %38
  store %struct.Book* %36, %struct.Book** %39, align 8
  br label %40

40:                                               ; preds = %16
  %41 = load i32, i32* %5, align 4
  %42 = add nsw i32 %41, 1
  store i32 %42, i32* %5, align 4
  br label %12

43:                                               ; preds = %12
  %44 = load i32, i32* %2, align 4
  %45 = sub nsw i32 %44, 1
  store i32 %45, i32* %7, align 4
  br label %46

46:                                               ; preds = %55, %43
  %47 = load i32, i32* %7, align 4
  %48 = icmp sge i32 %47, 0
  br i1 %48, label %49, label %58

49:                                               ; preds = %46
  %50 = load i32, i32* %7, align 4
  %51 = sext i32 %50 to i64
  %52 = getelementptr inbounds %struct.Book*, %struct.Book** %11, i64 %51
  %53 = load %struct.Book*, %struct.Book** %52, align 8
  %54 = bitcast %struct.Book* %53 to i8*
  call void @jefree(i8* %54)
  br label %55

55:                                               ; preds = %49
  %56 = load i32, i32* %7, align 4
  %57 = add nsw i32 %56, -1
  store i32 %57, i32* %7, align 4
  br label %46

58:                                               ; preds = %46
  store i32 0, i32* %1, align 4
  %59 = load i8*, i8** %3, align 8
  call void @llvm.stackrestore(i8* %59)
  %60 = load i32, i32* %1, align 4
  ret i32 %60
}

; Function Attrs: nounwind
declare i8* @llvm.stacksave() #1

; Function Attrs: nounwind
declare dso_local noalias i8* @malloc(i64) #2

; Function Attrs: nounwind
declare dso_local i8* @strcpy(i8*, i8*) #2

; Function Attrs: nounwind
declare dso_local void @free(i8*) #2

; Function Attrs: nounwind
declare void @llvm.stackrestore(i8*) #1

declare i8* @jemalloc(i64)

declare void @jefree(i8*)

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind }
attributes #2 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 9.0.0 (git@github.com:scampanoni/LLVM_installer.git 713d2f6594d9a0b77e7f9a120aaa7c917715a640)"}
