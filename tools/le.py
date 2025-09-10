import re
from lattice_estimator.estimator import *
from lattice_estimator.estimator.lwe_parameters import LWEParameters
from lattice_estimator.estimator.nd import DiscreteGaussianAlpha, DiscreteGaussian
from sage.all import Infinity
import math

def get_lambda(n=630, xs_sigma=0.5, xs_mu=0.50, xe_sigma=131072.00, xe_mu=0.0):
    params = LWEParameters(
        n=n,
        q=2**32,
        Xs=DiscreteGaussian(xs_sigma, xs_mu),
        Xe=DiscreteGaussian(xe_sigma, xe_mu),
        m=+Infinity
    )
    r = LWE.primal_usvp(params)
    output = r.str(compact=True)
    match = re.search(r'rop: ≈2\^([0-9.]+)', output)
    if match:
        exponent = int(math.ceil(float(match.group(1)))) + 3
        # print(f"ROP exponent: {exponent}")
        return exponent
    return None

    

if __name__ == "__main__":
    get_lambda()