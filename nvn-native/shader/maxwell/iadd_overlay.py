"""Isolated shader-compiler fix; never modify the shared vendored compiler.

Maxwell IADD.X uses a one's complement source modifier, unlike normal IADD.
Reference: pinned Mesa NAK sm50.rs OpIAdd2 / OpIAdd2X and hw_tests.rs.
The carry from the initial two's complement subtraction includes its +1.
"""
import hashlib

SOURCE_SHA = '11c2e58c7b2390c6d3b87caa3cd3cc17a65b3471933a4609b0aff703ffd40632'

OLD = '''            srcA = context.INegate(srcA, avgMode == AvgMode.NegA);
            srcB = context.INegate(srcB, avgMode == AvgMode.NegB);

            Operand res = context.IAdd(srcA, srcB);

            if (extended)
            {
                res = context.IAdd(res, context.BitwiseAnd(GetCF(), Const(1)));
            }

            SetIaddFlags(context, res, srcA, srcB, writeCC, extended);'''

NEW = '''            if (avgMode == AvgMode.PlusOne)
            {
                context.TranslatorContext.GpuAccessor.Log("IADD.PO is not qualified by this compiler profile.");
            }
            bool complementA = avgMode == AvgMode.NegA;
            bool complementB = avgMode == AvgMode.NegB;
            srcA = complementA ? context.BitwiseNot(srcA) : srcA;
            srcB = complementB ? context.BitwiseNot(srcB) : srcB;

            // IADD: ~a + b + 1; IADD.X: ~a + b + carry_in.
            Operand carryIn = extended ? context.BitwiseAnd(GetCF(), Const(1)) :
                Const(complementA || complementB ? 1 : 0);
            Operand sum = context.IAdd(srcA, srcB);
            Operand res = context.IAdd(sum, carryIn);
            if (writeCC)
            {
                Operand carry0 = context.ICompareLessUnsigned(sum, srcA);
                Operand carry1 = context.ICompareLessUnsigned(res, sum);
                context.Copy(GetCF(), context.BitwiseOr(carry0, carry1));
                Operand signs = context.BitwiseAnd(context.BitwiseExclusiveOr(res, srcA),
                    context.BitwiseNot(context.BitwiseExclusiveOr(srcA, srcB)));
                context.Copy(GetVF(), context.ICompareLess(signs, Const(0)));
                SetZnFlags(context, res, true, extended);
            }'''


def patch(source):
    if hashlib.sha256(source).hexdigest() != SOURCE_SHA:
        raise ValueError('IADD source changed; audit the isolated overlay against the new compiler')
    text = source.decode()
    if text.count(OLD) != 1:
        raise ValueError('IADD overlay must match exactly one instruction implementation')
    marker = '''
namespace Ryujinx.Graphics.Shader
{
    public static class NvnCompilerProfile
    {
        public const string Id = "maxwell-iadd-carry-v1";
    }
}
'''
    return (text.replace(OLD, NEW) + marker).encode()
