#include "hal/reset.h"

static ResetCausa_t reset_causa = RESET_CAUSA_DESCONOCIDA;

/* ------------------------------------------------------------------ */
void RESET_LeerCausa(void)
{
    uint32_t csr = RCC->CSR;

    /* Las banderas pueden venir combinadas, así que el orden decide qué se informa. `PORRSTF` va primero
       a propósito: cuando el riel cae despacio, el pin NRST puede dispararse antes que el detector de
       tensión y dejar las dos puestas, y en ese caso el problema está en la alimentación, no en el cable
       de NRST. Un botón de reset pulsado con la alimentación estable deja `PINRSTF` a solas. */
    if ((csr & RCC_CSR_PORRSTF) != 0U) {
        reset_causa = RESET_CAUSA_TENSION;
    } else if ((csr & RCC_CSR_PINRSTF) != 0U) {
        reset_causa = RESET_CAUSA_NRST;
    } else if ((csr & RCC_CSR_SFTRSTF) != 0U) {
        reset_causa = RESET_CAUSA_SOFTWARE;
    } else if ((csr & (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF)) != 0U) {
        reset_causa = RESET_CAUSA_WATCHDOG;
    } else if ((csr & RCC_CSR_LPWRRSTF) != 0U) {
        reset_causa = RESET_CAUSA_BAJO_CONSUMO;
    } else {
        reset_causa = RESET_CAUSA_DESCONOCIDA;
    }

    /* Se borran ya para que el próximo arranque no arrastre las banderas de este y diga dos causas. */
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

/* ------------------------------------------------------------------ */
ResetCausa_t RESET_GetCausa(void)
{
    return reset_causa;
}
