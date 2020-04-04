@.str = private unnamed_addr constant [4 x i8] c"abc\00", align 1

define dso_local i32 @Add(i32, i32)
{
    %3 = add i32 %0, %1
    ret i32 %3
}

define dso_local void @Store(i32, i32*)
{
    store i32 %0, i32* %1
    ret void
}

define dso_local i32 @Load(i32*)
{
    %2 = load i32, i32* %0
    ret i32 %2
}

define dso_local i32 @Main()
{
    call void asm "message(STATUS abc)", ""()
    %1 = call i32 @Add(i32 1, i32 2)
    ret i32 %1
}
