@.str = private unnamed_addr constant [4 x i8] c"abc\00", align 1
@.list = private unnamed_addr constant [4 x i32] [i32 1, i32 2, i32 3, i32 4], align 16

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
    %elemptr = getelementptr inbounds [4 x i32], [4 x i32]* @.list, i64 0, i64 1
    %elem = load i32, i32* %elemptr 
    %1 = call i32 @Add(i32 1, i32 2)
    ret i32 %1
}

define dso_local i32 @Compare(i1, i32, i32)
{
    %retValue = alloca i32, i32 1
    br i1 %0, label %trueBranch, label %falseBranch
trueBranch:
    store i32 %1, i32* %retValue
    br label %retBranch
falseBranch:
    store i32 %2, i32* %retValue
    br label %retBranch
retBranch:
    %4 = load i32, i32* %retValue
    ret i32 %4
}
