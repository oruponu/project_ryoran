import math

PAIR_SCORES = (0.0, 0.25, 0.5, 0.75, 1.0)
GAMES_PER_PAIR = 2
REGULARIZATION = 1e-3
DEFAULT_ALPHA = 0.05
DEFAULT_BETA = 0.05
SCORE_CLAMP = 1e-6
NEWTON_ITERATIONS = 100
NEWTON_TOLERANCE = 1e-12
CONFIDENCE_Z = 1.96


def expected_score(elo):
    return 1.0 / (1.0 + 10.0 ** (-elo / 400.0))


def elo_from_score(score):
    score = min(max(score, SCORE_CLAMP), 1.0 - SCORE_CLAMP)
    return -400.0 * math.log10(1.0 / score - 1.0)


def pair_index(engine1_won, engine2_won, draw):
    if min(engine1_won, engine2_won, draw) < 0 or engine1_won + engine2_won + draw != GAMES_PER_PAIR:
        raise ValueError(f"a pair must have {GAMES_PER_PAIR} games: {engine1_won}-{engine2_won}-{draw}")
    return 2 * engine1_won + draw


def mle_distribution(probabilities, score):
    deviations = [x - score for x in PAIR_SCORES]
    low = -1.0 / (1.0 - score)
    high = 1.0 / score
    lam = 0.0
    for _ in range(NEWTON_ITERATIONS):
        value = sum(p * d / (1.0 + lam * d) for p, d in zip(probabilities, deviations))
        if abs(value) < NEWTON_TOLERANCE:
            break
        if value > 0.0:
            low = lam
        else:
            high = lam
        slope = -sum(p * d * d / (1.0 + lam * d) ** 2 for p, d in zip(probabilities, deviations))
        candidate = lam - value / slope
        lam = candidate if low < candidate < high else (low + high) / 2.0
    return [p / (1.0 + lam * d) for p, d in zip(probabilities, deviations)]


def llr(counts, elo0, elo1):
    regularized = [n + REGULARIZATION for n in counts]
    total = sum(regularized)
    probabilities = [n / total for n in regularized]
    p0 = mle_distribution(probabilities, expected_score(elo0))
    p1 = mle_distribution(probabilities, expected_score(elo1))
    return sum(n * math.log(a / b) for n, a, b in zip(counts, p1, p0))


def bounds(alpha=DEFAULT_ALPHA, beta=DEFAULT_BETA):
    return math.log(beta / (1.0 - alpha)), math.log((1.0 - beta) / alpha)


def decide(llr_value, alpha=DEFAULT_ALPHA, beta=DEFAULT_BETA):
    lower, upper = bounds(alpha, beta)
    if llr_value >= upper:
        return "H1"
    if llr_value <= lower:
        return "H0"
    return None


def elo_estimate(counts):
    total = sum(counts)
    if total == 0:
        return None
    mean = sum(n * x for n, x in zip(counts, PAIR_SCORES)) / total
    variance = sum(n * (x - mean) ** 2 for n, x in zip(counts, PAIR_SCORES)) / total
    half_width = CONFIDENCE_Z * math.sqrt(variance / total)
    return elo_from_score(mean), elo_from_score(mean - half_width), elo_from_score(mean + half_width)
